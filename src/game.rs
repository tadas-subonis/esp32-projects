//! Snake game logic using Bevy ECS.
//!
//! Contains all ECS components, resources, and systems for the snake game.

use bevy_ecs::prelude::*;
use core::fmt::Write;
use embedded_graphics::{
    mono_font::ascii::FONT_10X20,
    pixelcolor::Rgb888,
    prelude::*,
};
use esp_hal::rng::Rng;
use heapless::String;

use crate::config::{
    AXP2101_ADDR, AXP2101_INTSTS2, AXP2101_PKEY_SHORT_IRQ_BIT, CELL_SIZE, GRID_HEIGHT,
    GRID_OFFSET_X, GRID_OFFSET_Y, GRID_WIDTH, HOLD_TO_RESTART_FRAMES, LCD_H_RES, LCD_V_RES,
};
use crate::display::FrameBufferResource;
use crate::hardware::{Axp2101Resource, ButtonLeftResource, DisplayResource};
use crate::perf::PerformanceMetrics;
use crate::render::{
    colors, draw_grid_cell, draw_grid_cell_animated, draw_grid_cell_gradient, draw_score,
    draw_text, draw_text_centered, transfer_framebuffer_rle, GridConfig,
};

// =============================================================================
// Components
// =============================================================================

#[derive(Component, Clone, Copy, PartialEq, Eq)]
pub struct Position {
    pub x: i32,
    pub y: i32,
}

#[derive(Component, Clone, Copy)]
pub enum Direction {
    Up,
    Down,
    Left,
    Right,
}

impl Direction {
    pub fn turn_left(self) -> Self {
        match self {
            Direction::Up => Direction::Left,
            Direction::Left => Direction::Down,
            Direction::Down => Direction::Right,
            Direction::Right => Direction::Up,
        }
    }

    pub fn turn_right(self) -> Self {
        match self {
            Direction::Up => Direction::Right,
            Direction::Right => Direction::Down,
            Direction::Down => Direction::Left,
            Direction::Left => Direction::Up,
        }
    }
}

#[derive(Component)]
pub struct SnakeHead;

#[derive(Component)]
pub struct SnakeSegment {
    pub index: usize,
}

#[derive(Component, Clone, Copy, PartialEq, Eq, Debug)]
pub enum FoodType {
    Regular, // 10 points
    Golden,  // 50 points
    Special, // 100 points
}

impl FoodType {
    pub fn points(&self) -> u32 {
        match self {
            FoodType::Regular => 10,
            FoodType::Golden => 50,
            FoodType::Special => 100,
        }
    }

    pub fn color(&self) -> Rgb888 {
        match self {
            FoodType::Regular => colors::RED,
            FoodType::Golden => colors::GOLD,
            FoodType::Special => colors::MAGENTA,
        }
    }

    pub fn spawn_chance(&self) -> u32 {
        match self {
            FoodType::Regular => 70, // 70% chance
            FoodType::Golden => 25,  // 25% chance
            FoodType::Special => 5,  // 5% chance
        }
    }
}

#[derive(Component)]
pub struct Food {
    pub food_type: FoodType,
    pub animation_frame: u8, // For pulsing animation
}

// =============================================================================
// Resources
// =============================================================================

#[derive(Resource)]
pub struct GameState {
    pub score: u32,
    pub game_over: bool,
    pub move_timer: u32,
    pub move_interval: u32,   // frames between moves
    pub frames_elapsed: u32,  // Track total frames to prevent immediate collision
    pub needs_redraw: bool,   // Track if we need to redraw
    pub new_high_score: bool, // Track if we just achieved a new high score
}

impl Default for GameState {
    fn default() -> Self {
        Self {
            score: 0,
            game_over: false,
            move_timer: 0,
            move_interval: 10, // Movement speed (frames between moves) - tuned for ~60fps target
            frames_elapsed: 0,
            needs_redraw: true, // Initial render needed
            new_high_score: false,
        }
    }
}

#[derive(Resource)]
pub struct RngResource(pub Rng);

#[derive(Resource)]
pub struct HighScore {
    score: u32,
    // TODO: Add flash persistence when esp-storage or similar is available
    // For now, high score resets on reboot
}

impl HighScore {
    pub fn new() -> Self {
        Self { score: 0 }
    }

    pub fn update(&mut self, new_score: u32) -> bool {
        if new_score > self.score {
            self.score = new_score;
            esp_println::println!("NEW HIGH SCORE: {}!", self.score);
            true
        } else {
            false
        }
    }

    pub fn get(&self) -> u32 {
        self.score
    }
}

impl Default for HighScore {
    fn default() -> Self {
        Self::new()
    }
}

#[derive(Resource, Default)]
pub struct InputState {
    pub turn_left: bool,
    pub turn_right: bool,
    pub restart: bool,
}

#[derive(Resource, Default)]
pub struct ButtonState {
    /// Was BOOT button pressed last frame (for edge detection)
    pub boot_was_pressed: bool,
    /// BOOT button debounce counter
    pub boot_debounce: u32,
    /// Was power key pressed last frame (for edge detection)
    pub pkey_was_pressed: bool,
    /// Frame counter for hold-to-restart
    pub hold_start_frame: Option<u32>,
    /// Current frame number
    pub frame: u32,
}

// =============================================================================
// Systems
// =============================================================================

/// Two-button input system:
/// - GPIO0 (BOOT button) = turn left
/// - AXP2101 Power Key (via PMU IRQ) = turn right
/// - Hold either when game over = restart
///
/// Uses EDGE DETECTION: only triggers on button press, not while held
pub fn input_system(
    left_btn: NonSendMut<ButtonLeftResource>,
    axp2101: NonSendMut<Axp2101Resource>,
    mut input_state: ResMut<InputState>,
    mut button_state: ResMut<ButtonState>,
    game_state: Res<GameState>,
) {
    button_state.frame += 1;

    // === BOOT BUTTON (GPIO0) - active-low ===
    let boot_raw = left_btn.button.is_low();

    // Minimal debounce: just 1 frame to avoid electrical noise
    // (Reduced from 3 frames for faster response)
    let boot_pressed = if boot_raw {
        button_state.boot_debounce = button_state.boot_debounce.saturating_add(1);
        button_state.boot_debounce >= 1
    } else {
        button_state.boot_debounce = 0;
        false
    };

    // Edge detection for BOOT button
    let boot_just_pressed = boot_pressed && !button_state.boot_was_pressed;
    button_state.boot_was_pressed = boot_pressed;

    // === POWER KEY (AXP2101 PMU) - read from IRQ status register ===
    // The power key press is detected via the PMU's interrupt status register.
    // We read INTSTS2, check the PKEY_SHORT bit, then clear it by writing 0xFF.
    let pkey_pressed = {
        let mut i2c = axp2101.i2c.borrow_mut();
        let mut status = [0u8; 1];

        // Read interrupt status register 2 (0x49)
        if i2c
            .write_read(AXP2101_ADDR, &[AXP2101_INTSTS2], &mut status)
            .is_ok()
        {
            let short_press = (status[0] & AXP2101_PKEY_SHORT_IRQ_BIT) != 0;

            // Clear the interrupt by writing 0xFF to the status register
            if short_press {
                let _ = i2c.write(AXP2101_ADDR, &[AXP2101_INTSTS2, 0xFF]);
            }

            short_press
        } else {
            false
        }
    };

    // Power key is edge-triggered by design (IRQ fires once per press)
    let pkey_just_pressed = pkey_pressed && !button_state.pkey_was_pressed;
    button_state.pkey_was_pressed = pkey_pressed;

    // === Combined input handling ===
    let any_pressed = boot_pressed || pkey_pressed;
    let _any_just_pressed = boot_just_pressed || pkey_just_pressed;

    // Handle game-over state: hold to restart
    if game_state.game_over {
        if any_pressed {
            if button_state.hold_start_frame.is_none() {
                button_state.hold_start_frame = Some(button_state.frame);
            } else if let Some(start) = button_state.hold_start_frame {
                let hold_duration = button_state.frame.saturating_sub(start);
                if hold_duration >= HOLD_TO_RESTART_FRAMES && !input_state.restart {
                    input_state.restart = true;
                    esp_println::println!("BTN: held long enough - restart!");
                }
            }
        } else {
            button_state.hold_start_frame = None;
        }
    } else {
        // During gameplay: BOOT = left, Power Key = right
        button_state.hold_start_frame = None;

        if boot_just_pressed {
            input_state.turn_left = true;
            esp_println::println!("BTN: BOOT press -> turn left");
        }
        if pkey_just_pressed {
            input_state.turn_right = true;
            esp_println::println!("BTN: Power key press -> turn right");
        }
    }
}

pub fn process_input_system(
    mut input_state: ResMut<InputState>,
    mut game_state: ResMut<GameState>,
    mut head_query: Query<&mut Direction, With<SnakeHead>>,
) {
    // Handle restart first (works even when game is over)
    if input_state.restart {
        return;
    }

    // Only process turn inputs if game is not over
    if game_state.game_over {
        input_state.turn_left = false;
        input_state.turn_right = false;
        return;
    }

    // Process turn inputs (edge-triggered in input_system, so this fires at most once per press)
    // Left takes priority if both pressed simultaneously
    if input_state.turn_left {
        if let Ok(mut dir) = head_query.single_mut() {
            *dir = dir.turn_left();
            game_state.needs_redraw = true;
        }
        input_state.turn_left = false;
    } else if input_state.turn_right {
        if let Ok(mut dir) = head_query.single_mut() {
            *dir = dir.turn_right();
            game_state.needs_redraw = true;
        }
        input_state.turn_right = false;
    }
}

pub fn move_snake_system(
    mut game_state: ResMut<GameState>,
    mut head_query: Query<(&mut Position, &Direction), With<SnakeHead>>,
    mut segment_query: Query<
        (&mut Position, &SnakeSegment),
        (With<SnakeSegment>, Without<SnakeHead>),
    >,
) {
    if game_state.game_over {
        return;
    }

    game_state.frames_elapsed += 1;
    game_state.move_timer += 1;
    if game_state.move_timer < game_state.move_interval {
        return;
    }
    game_state.move_timer = 0;
    game_state.needs_redraw = true; // Mark that we need to redraw after movement

    let (mut head_pos, direction) = match head_query.single_mut() {
        Ok(h) => h,
        Err(_) => return,
    };

    // Calculate new head position
    let new_head_pos = match *direction {
        Direction::Up => Position {
            x: head_pos.x,
            y: head_pos.y - 1,
        },
        Direction::Down => Position {
            x: head_pos.x,
            y: head_pos.y + 1,
        },
        Direction::Left => Position {
            x: head_pos.x - 1,
            y: head_pos.y,
        },
        Direction::Right => Position {
            x: head_pos.x + 1,
            y: head_pos.y,
        },
    };

    // Store old head position BEFORE any updates
    let old_head_pos = *head_pos;

    // CRITICAL: Collect all segment data (index, old_position) BEFORE updating head or any segments
    // This ensures we have a snapshot of all positions before any mutations
    let mut segment_data: heapless::Vec<(usize, Position), 256> = heapless::Vec::new();
    for (pos, seg) in segment_query.iter() {
        segment_data.push((seg.index, *pos)).ok();
    }

    // Now update head to new position (after collecting segment positions)
    *head_pos = new_head_pos;

    // 2. Sort by index (ascending: 0, 1, 2...)
    segment_data.sort_unstable_by_key(|(idx, _)| *idx);

    // Validate segment_data: check for duplicate indices or missing indices
    let mut prev_idx = None;
    for (idx, _) in segment_data.iter() {
        if let Some(prev) = prev_idx {
            if *idx == prev {
                esp_println::println!("MOVE: ERROR - duplicate segment index {}", idx);
            } else if *idx != prev + 1 {
                esp_println::println!(
                    "MOVE: WARNING - non-contiguous indices: {} -> {}",
                    prev,
                    idx
                );
            }
        }
        prev_idx = Some(*idx);
    }

    // Debug: log collected segment data occasionally (kept lightweight)
    if !segment_data.is_empty() && game_state.frames_elapsed % 200 == 0 {
        esp_println::println!(
            "MOVE: head old=({}, {}) new=({}, {}) segs={}",
            old_head_pos.x,
            old_head_pos.y,
            new_head_pos.x,
            new_head_pos.y,
            segment_data.len()
        );
    }

    // 3. Update segments in REVERSE order (tail to head) to avoid overwriting positions we still need
    // This ensures we never overwrite a position that another segment still needs
    // By updating from tail to head, each segment gets the position of the segment ahead of it,
    // and we never overwrite a position we still need to read
    let max_index = segment_data.iter().map(|(idx, _)| *idx).max().unwrap_or(0);

    // Update segments in reverse order (highest index first, down to 0)
    for target_idx in (0..=max_index).rev() {
        // Find the segment with this index and update it
        for (mut pos, seg) in segment_query.iter_mut() {
            if seg.index == target_idx {
                if target_idx == 0 {
                    // Segment 0 gets the old head position
                    *pos = old_head_pos;
                } else {
                    // Segment i gets the old position of segment i-1
                    // Find the old position of segment (target_idx - 1) in segment_data
                    if let Some((_prev_idx, prev_pos)) =
                        segment_data.iter().find(|(idx, _)| *idx == target_idx - 1)
                    {
                        // Critical check: if the previous segment's old position is the new head position,
                        // this indicates a serious bug in the movement logic
                        if *prev_pos == new_head_pos {
                            esp_println::println!("MOVE: CRITICAL ERROR - segment {} prev_seg[{}] old_pos=({}, {}) == new_head=({}, {})", 
                                target_idx, target_idx - 1, prev_pos.x, prev_pos.y, new_head_pos.x, new_head_pos.y);
                            esp_println::println!(
                                "MOVE: This should never happen! segment_data may be corrupted."
                            );
                            // Don't update - leave segment at current position to avoid crash
                        } else if *prev_pos == *pos {
                            // Another safety check: if we're trying to move a segment to where it already is,
                            // something is wrong (this can happen if segment_data is stale)
                            esp_println::println!(
                                "MOVE: WARNING - segment {} already at target position ({}, {})",
                                target_idx,
                                prev_pos.x,
                                prev_pos.y
                            );
                            // Still update to be safe, but log the warning
                            *pos = *prev_pos;
                        } else {
                            *pos = *prev_pos;

                            // Final safety check: ensure no segment ends up at the new head position
                            if *pos == new_head_pos {
                                esp_println::println!(
                                    "MOVE: CRITICAL ERROR - segment {} ended up at new head position ({}, {})!",
                                    target_idx,
                                    pos.x,
                                    pos.y
                                );
                                // Move it to a safe position (old head position as fallback)
                                *pos = old_head_pos;
                            }

                            // Debug: log segment updates occasionally
                            if target_idx <= 2 && game_state.frames_elapsed % 50 == 0 {
                                esp_println::println!(
                                    "MOVE: updated segment {} to ({}, {})",
                                    target_idx,
                                    pos.x,
                                    pos.y
                                );
                            }
                        }
                    } else {
                        esp_println::println!(
                            "MOVE: ERROR - segment {} could not find previous segment {} in segment_data",
                            target_idx,
                            target_idx - 1
                        );
                    }
                }

                // Found this segment (whether updated or not), move to next index
                break;
            }
        }
    }
}

fn dump_game_state(
    reason: &str,
    head_pos: &Position,
    head_dir: &Direction,
    game_state: &GameState,
    segment_query: &Query<(&Position, &SnakeSegment), (With<SnakeSegment>, Without<SnakeHead>)>,
    food_query: &Query<(Entity, &Position, &Food), With<Food>>,
) {
    let dir_str = match head_dir {
        Direction::Up => "Up",
        Direction::Down => "Down",
        Direction::Left => "Left",
        Direction::Right => "Right",
    };

    esp_println::println!("=== GAME OVER: {} ===", reason);
    esp_println::println!(
        "  Head: ({}, {}) facing {}",
        head_pos.x,
        head_pos.y,
        dir_str
    );
    esp_println::println!(
        "  Grid: {}x{} (valid: 0..{}, 0..{})",
        GRID_WIDTH,
        GRID_HEIGHT,
        GRID_WIDTH - 1,
        GRID_HEIGHT - 1
    );
    esp_println::println!(
        "  Score: {}, Frames: {}, Speed: {}",
        game_state.score,
        game_state.frames_elapsed,
        game_state.move_interval
    );

    // Collect and sort segments by index
    let mut segments: heapless::Vec<(usize, i32, i32), 64> = heapless::Vec::new();
    for (pos, seg) in segment_query.iter() {
        segments.push((seg.index, pos.x, pos.y)).ok();
    }
    segments.sort_unstable_by_key(|(idx, _, _)| *idx);

    esp_println::println!("  Segments ({}):", segments.len());
    for (idx, x, y) in segments.iter() {
        esp_println::println!("    [{}]: ({}, {})", idx, x, y);
    }

    // Food positions
    let mut food_count = 0;
    for (_, food_pos, food) in food_query.iter() {
        esp_println::println!(
            "  Food: ({}, {}) type={:?}",
            food_pos.x,
            food_pos.y,
            food.food_type
        );
        food_count += 1;
    }
    if food_count == 0 {
        esp_println::println!("  Food: none");
    }
    esp_println::println!("=== END STATE DUMP ===");
}

pub fn collision_system(
    mut game_state: ResMut<GameState>,
    mut high_score: ResMut<HighScore>,
    head_query: Query<(&Position, &Direction), With<SnakeHead>>,
    segment_query: Query<(&Position, &SnakeSegment), (With<SnakeSegment>, Without<SnakeHead>)>,
    food_query: Query<(Entity, &Position, &Food), With<Food>>,
    mut commands: Commands,
) {
    if game_state.game_over {
        return;
    }

    // Don't check collisions until snake has moved at least once
    // This prevents false collisions on the first frame
    if game_state.frames_elapsed < game_state.move_interval {
        return;
    }

    let (head_pos, head_dir) = match head_query.single() {
        Ok((p, d)) => (*p, *d),
        Err(_) => return,
    };

    // Check wall collision
    if head_pos.x < 0 || head_pos.x >= GRID_WIDTH || head_pos.y < 0 || head_pos.y >= GRID_HEIGHT {
        game_state.game_over = true;
        game_state.needs_redraw = true;
        // Check for new high score on game over
        if high_score.update(game_state.score) {
            game_state.new_high_score = true;
        }
        dump_game_state(
            "Wall collision",
            &head_pos,
            &head_dir,
            &game_state,
            &segment_query,
            &food_query,
        );
        return;
    }

    // Check self collision
    // Skip segment 0 (index 0) because it's always at the old head position right after movement
    // This prevents false collisions. Only check segments 1 and beyond.
    for (seg_pos, seg) in segment_query.iter() {
        if seg.index > 0 && *seg_pos == head_pos {
            game_state.game_over = true;
            game_state.needs_redraw = true;
            // Check for new high score on game over
            if high_score.update(game_state.score) {
                game_state.new_high_score = true;
            }
            esp_println::println!(
                "=== GAME OVER: Self collision with segment {} ===",
                seg.index
            );
            dump_game_state(
                "Self collision",
                &head_pos,
                &head_dir,
                &game_state,
                &segment_query,
                &food_query,
            );
            return;
        }
    }

    // Check food collision
    for (food_entity, food_pos, food) in food_query.iter() {
        if *food_pos == head_pos {
            // Eat food - get points based on food type
            let points = food.food_type.points();
            commands.entity(food_entity).despawn();
            game_state.score += points;

            // Add new segment to snake
            // Find the last segment (highest index)
            // Use index to find tail, not position order which can be misleading if coiled
            let max_index_seg = segment_query.iter().max_by_key(|(_, s)| s.index);

            let (last_pos, next_index) = if let Some((pos, s)) = max_index_seg {
                (*pos, s.index + 1)
            } else {
                (head_pos, 0)
            };

            // Safety check: ensure new segment is not spawned at head position
            // This should never happen, but if it does, spawn it at a safe position
            let spawn_pos = if last_pos == head_pos {
                esp_println::println!(
                    "FOOD: WARNING - tail at head position ({}, {}), using fallback",
                    head_pos.x,
                    head_pos.y
                );
                // Fallback: spawn behind the head in the opposite direction of movement
                // This is a safety measure and should never be needed
                match head_dir {
                    Direction::Up => Position {
                        x: head_pos.x,
                        y: head_pos.y + 1,
                    },
                    Direction::Down => Position {
                        x: head_pos.x,
                        y: head_pos.y - 1,
                    },
                    Direction::Left => Position {
                        x: head_pos.x + 1,
                        y: head_pos.y,
                    },
                    Direction::Right => Position {
                        x: head_pos.x - 1,
                        y: head_pos.y,
                    },
                }
            } else {
                last_pos
            };

            commands.spawn((SnakeSegment { index: next_index }, spawn_pos));

            // Speed up slightly (more for special foods)
            let speed_boost = match food.food_type {
                FoodType::Regular => 1,
                FoodType::Golden => 1,
                FoodType::Special => 2,
            };
            for _ in 0..speed_boost {
                if game_state.move_interval > 2 {
                    game_state.move_interval -= 1;
                }
            }
            game_state.needs_redraw = true;

            esp_println::println!(
                "Score: {} (+{} from {:?})",
                game_state.score,
                points,
                food.food_type
            );
        }
    }
}

pub fn restart_system(
    mut input_state: ResMut<InputState>,
    mut game_state: ResMut<GameState>,
    mut head_query: Query<&mut Position, With<SnakeHead>>,
    mut head_dir_query: Query<&mut Direction, With<SnakeHead>>,
    segment_query: Query<Entity, With<SnakeSegment>>,
    food_query: Query<Entity, With<Food>>,
    mut commands: Commands,
) {
    if !input_state.restart {
        return;
    }

    esp_println::println!("=== RESTART SYSTEM TRIGGERED ===");
    esp_println::println!("  game_over was: {}", game_state.game_over);

    // Clear restart flag immediately to prevent multiple restarts
    input_state.restart = false;

    // Reset game state (preserve high score)
    let old_high_score = game_state.new_high_score;
    *game_state = GameState::default();
    game_state.new_high_score = old_high_score; // Preserve flag during restart

    // Remove all segments
    for entity in segment_query.iter() {
        commands.entity(entity).despawn();
    }

    // Remove all food
    for entity in food_query.iter() {
        commands.entity(entity).despawn();
    }

    // Reset head position and direction
    if let Ok(mut head_pos) = head_query.single_mut() {
        *head_pos = Position { x: 23, y: 28 }; // Center of grid
    }
    if let Ok(mut dir) = head_dir_query.single_mut() {
        *dir = Direction::Right;
    }

    // Spawn initial segments
    commands.spawn((SnakeSegment { index: 0 }, Position { x: 22, y: 28 }));
    commands.spawn((SnakeSegment { index: 1 }, Position { x: 21, y: 28 }));

    game_state.needs_redraw = true;
    esp_println::println!("Game restarted!");
}

pub fn food_spawn_system(
    mut commands: Commands,
    food_query: Query<Entity, With<Food>>,
    all_positions: Query<&Position>,
    rng: ResMut<RngResource>,
    game_state: Res<GameState>,
) {
    if game_state.game_over {
        return;
    }

    // Only spawn food if none exists
    if food_query.iter().next().is_some() {
        return;
    }

    // Determine food type based on weighted random
    let mut buf = [0u8; 4];
    rng.0.read(&mut buf);
    let rand_val = (buf[0] as u32 | ((buf[1] as u32) << 8)) % 100;

    let food_type = if rand_val < FoodType::Regular.spawn_chance() {
        FoodType::Regular
    } else if rand_val < FoodType::Regular.spawn_chance() + FoodType::Golden.spawn_chance() {
        FoodType::Golden
    } else {
        FoodType::Special
    };

    // Try to find a free position
    let mut attempts = 0;
    loop {
        rng.0.read(&mut buf);
        let x = ((buf[0] as u32 | ((buf[1] as u32) << 8)) % GRID_WIDTH as u32) as i32;
        rng.0.read(&mut buf);
        let y = ((buf[0] as u32 | ((buf[1] as u32) << 8)) % GRID_HEIGHT as u32) as i32;

        let pos = Position { x, y };
        let occupied = all_positions.iter().any(|&p| p == pos);

        if !occupied {
            commands.spawn((
                Food {
                    food_type,
                    animation_frame: 0,
                },
                pos,
            ));
            esp_println::println!("Food spawned: {:?} at ({}, {})", food_type, x, y);
            break;
        }

        attempts += 1;
        if attempts > 100 {
            // Give up if we can't find a spot
            break;
        }
    }
}

pub fn animate_food_system(mut food_query: Query<&mut Food>) {
    for mut food in food_query.iter_mut() {
        food.animation_frame = food.animation_frame.wrapping_add(1);
    }
}

// =============================================================================
// Render System
// =============================================================================

/// Snake head color (darker green)
const SNAKE_HEAD_COLOR: Rgb888 = Rgb888::new(0, 200, 0);
/// Snake body base color (bright green)
const SNAKE_BODY_COLOR: Rgb888 = Rgb888::new(0, 255, 0);

/// Render food items to the framebuffer.
fn render_food(fb_data: &mut [Rgb888], config: &GridConfig, food_query: &Query<(&Position, &Food), With<Food>>) {
    for (food_pos, food) in food_query.iter() {
        draw_grid_cell_animated(
            fb_data,
            config,
            food_pos.x,
            food_pos.y,
            food.food_type.color(),
            food.animation_frame,
        );
    }
}

/// Render snake segments to the framebuffer with gradient effect.
fn render_snake_segments(
    fb_data: &mut [Rgb888],
    config: &GridConfig,
    segment_query: &Query<(&Position, &SnakeSegment), With<SnakeSegment>>,
) {
    // Collect all segments with their indices
    let mut segments: heapless::Vec<(usize, i32, i32), 256> = heapless::Vec::new();
    for (seg_pos, seg) in segment_query.iter() {
        segments.push((seg.index, seg_pos.x, seg_pos.y)).ok();
    }
    segments.sort_unstable_by_key(|(idx, _, _)| *idx);
    let total_segments = segments.len().max(1);

    // Draw segments with gradient based on position in snake
    for (idx, seg_x, seg_y) in segments.iter() {
        // Calculate gradient: head (index 0) is brightest, tail is darkest
        // Intensity ranges from 255 (head) to 128 (tail)
        let intensity = 128 + ((127 * (total_segments - idx)) / total_segments.max(1)) as u8;
        draw_grid_cell_gradient(fb_data, config, *seg_x, *seg_y, SNAKE_BODY_COLOR, intensity);
    }
}

/// Render the snake head to the framebuffer.
fn render_snake_head(
    fb_data: &mut [Rgb888],
    config: &GridConfig,
    head_query: &Query<&Position, With<SnakeHead>>,
) {
    if let Ok(head_pos) = head_query.single() {
        draw_grid_cell(fb_data, config, head_pos.x, head_pos.y, SNAKE_HEAD_COLOR);
    }
}

/// Render the game over screen UI.
fn render_game_over_ui<D: DrawTarget<Color = Rgb888>>(
    target: &mut D,
    game_state: &GameState,
    high_score: &HighScore,
) {
    let game_over_text = if game_state.new_high_score {
        "NEW HIGH SCORE!"
    } else {
        "GAME OVER"
    };

    let center_y = LCD_V_RES as i32 / 2;

    // Draw title
    let title_color = if game_state.new_high_score {
        colors::GOLD
    } else {
        colors::RED
    };
    draw_text_centered(
        target,
        game_over_text,
        center_y - 50,
        title_color,
        &FONT_10X20,
        LCD_H_RES as i32,
        10,
    );

    // Draw current score
    let mut score_text = String::<20>::new();
    write!(score_text, "Score: {}", game_state.score).ok();
    draw_text_centered(
        target,
        score_text.as_str(),
        center_y - 20,
        colors::WHITE,
        &FONT_10X20,
        LCD_H_RES as i32,
        10,
    );

    // Draw high score
    let mut high_score_text = String::<30>::new();
    write!(high_score_text, "High: {}", high_score.get()).ok();
    draw_text_centered(
        target,
        high_score_text.as_str(),
        center_y + 10,
        colors::GOLD,
        &FONT_10X20,
        LCD_H_RES as i32,
        10,
    );

    // Draw restart instructions
    draw_text_centered(
        target,
        "Hold both to restart",
        center_y + 40,
        colors::GRAY,
        &FONT_10X20,
        LCD_H_RES as i32,
        10,
    );
}

/// Render the in-game HUD (score, high score, instructions).
fn render_gameplay_ui<D: DrawTarget<Color = Rgb888>>(
    target: &mut D,
    game_state: &GameState,
    high_score: &HighScore,
) {
    // Draw score
    draw_score(
        target,
        "Score: ",
        game_state.score,
        8,
        LCD_V_RES as i32 - 40,
        colors::WHITE,
    );

    // Draw high score
    draw_score(
        target,
        "High: ",
        high_score.get(),
        8,
        LCD_V_RES as i32 - 20,
        colors::GOLD,
    );

    // Draw instructions
    draw_text(
        target,
        "Boot=Left Pwr=Right",
        8,
        8,
        colors::GRAY,
        &FONT_10X20,
    );
}

/// Main render system for the snake game.
///
/// This system renders:
/// - Food items with pulsing animation
/// - Snake body with gradient effect
/// - Snake head
/// - UI elements (score, high score, game over screen)
pub fn render_system(
    mut display_res: NonSendMut<DisplayResource>,
    mut game_state: ResMut<GameState>,
    mut fb_res: ResMut<FrameBufferResource>,
    head_query: Query<&Position, With<SnakeHead>>,
    segment_query: Query<(&Position, &SnakeSegment), With<SnakeSegment>>,
    food_query: Query<(&Position, &Food), With<Food>>,
    high_score: Res<HighScore>,
    _perf_metrics: Option<Res<PerformanceMetrics>>,
) {
    // Only render when needed (after movement or state change)
    if !game_state.needs_redraw {
        return;
    }
    game_state.needs_redraw = false;

    // Clear framebuffer
    fb_res.frame_buf.clear(Rgb888::BLACK).unwrap();

    // Get grid config
    let config = GridConfig::new(
        LCD_H_RES,
        LCD_V_RES,
        CELL_SIZE as usize,
        GRID_OFFSET_X,
        GRID_OFFSET_Y,
    );

    // Access framebuffer data
    let fb_data: &mut [Rgb888] = &mut *fb_res.frame_buf.data;

    // Render game objects
    render_food(fb_data, &config, &food_query);
    render_snake_segments(fb_data, &config, &segment_query);
    render_snake_head(fb_data, &config, &head_query);

    // Render UI
    if game_state.game_over {
        render_game_over_ui(&mut fb_res.frame_buf, &game_state, &high_score);
    } else {
        render_gameplay_ui(&mut fb_res.frame_buf, &game_state, &high_score);
    }

    // Transfer framebuffer to display
    let fb_data: &[Rgb888] = &*fb_res.frame_buf.data;
    transfer_framebuffer_rle(&mut display_res.display, fb_data, LCD_H_RES, LCD_V_RES);

    display_res.display.flush().ok();
}
