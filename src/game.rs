//! Snake game logic implemented without ECS.
//!
//! The module keeps the game behavior identical to the former Bevy-based
//! version while using lightweight structs suitable for `no_std` on ESP32.

use embedded_graphics::pixelcolor::Rgb888;
use esp_hal::rng::Rng;
use heapless::Vec;

use crate::config::{
    AXP2101_ADDR, AXP2101_INTSTS2, AXP2101_PKEY_SHORT_IRQ_BIT, GRID_HEIGHT, GRID_WIDTH,
    HOLD_TO_RESTART_FRAMES,
};
use crate::hardware::{Axp2101Resource, ButtonLeftResource};

const MAX_SEGMENTS: usize = 256;

#[derive(Clone, Copy, PartialEq, Eq)]
pub struct Position {
    pub x: i32,
    pub y: i32,
}

#[derive(Clone, Copy)]
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

#[derive(Clone, Copy, PartialEq, Eq, Debug)]
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
            FoodType::Regular => Rgb888::new(255, 0, 0),
            FoodType::Golden => Rgb888::new(255, 215, 0),
            FoodType::Special => Rgb888::new(255, 0, 255),
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

#[derive(Clone, Copy)]
pub struct Food {
    pub food_type: FoodType,
    pub animation_frame: u8, // For pulsing animation
}

#[derive(Clone, Copy)]
pub struct FoodEntity {
    pub position: Position,
    pub food: Food,
}

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

#[derive(Default)]
pub struct InputState {
    pub turn_left: bool,
    pub turn_right: bool,
    pub restart: bool,
}

#[derive(Default)]
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

struct Snake {
    head: Position,
    direction: Direction,
    segments: Vec<Position, MAX_SEGMENTS>,
}

impl Snake {
    fn new() -> Self {
        let mut segments: Vec<Position, MAX_SEGMENTS> = Vec::new();
        segments.push(Position { x: 22, y: 28 }).ok();
        segments.push(Position { x: 21, y: 28 }).ok();
        Self {
            head: Position { x: 23, y: 28 },
            direction: Direction::Right,
            segments,
        }
    }

    fn move_forward(&mut self, game_state: &mut GameState) {
        game_state.frames_elapsed = game_state.frames_elapsed.saturating_add(1);
        game_state.move_timer = game_state.move_timer.saturating_add(1);
        if game_state.move_timer < game_state.move_interval {
            return;
        }
        game_state.move_timer = 0;
        game_state.needs_redraw = true;

        let new_head = match self.direction {
            Direction::Up => Position {
                x: self.head.x,
                y: self.head.y - 1,
            },
            Direction::Down => Position {
                x: self.head.x,
                y: self.head.y + 1,
            },
            Direction::Left => Position {
                x: self.head.x - 1,
                y: self.head.y,
            },
            Direction::Right => Position {
                x: self.head.x + 1,
                y: self.head.y,
            },
        };

        let mut prev_pos = self.head;
        for seg in self.segments.iter_mut() {
            core::mem::swap(seg, &mut prev_pos);
        }
        self.head = new_head;
    }

    fn grow(&mut self, position: Position) {
        let _ = self.segments.push(position);
    }

    fn head(&self) -> Position {
        self.head
    }

    fn direction(&self) -> Direction {
        self.direction
    }

    fn set_direction(&mut self, dir: Direction) {
        self.direction = dir;
    }

    fn segments(&self) -> &[Position] {
        &self.segments
    }
}

pub struct Game {
    pub state: GameState,
    pub high_score: HighScore,
    pub input: InputState,
    pub button_state: ButtonState,
    pub rng: Rng,
    snake: Snake,
    food: Option<FoodEntity>,
}

impl Game {
    pub fn new(rng: Rng) -> Self {
        Self {
            state: GameState::default(),
            high_score: HighScore::new(),
            input: InputState::default(),
            button_state: ButtonState::default(),
            rng,
            snake: Snake::new(),
            food: None,
        }
    }

    pub fn poll_inputs(
        &mut self,
        left_btn: &ButtonLeftResource,
        axp2101: &mut Axp2101Resource,
    ) {
        self.button_state.frame = self.button_state.frame.saturating_add(1);

        // BOOT button (GPIO0) - active-low
        let boot_raw = left_btn.button.is_low();
        let boot_pressed = if boot_raw {
            self.button_state.boot_debounce = self.button_state.boot_debounce.saturating_add(1);
            self.button_state.boot_debounce >= 1
        } else {
            self.button_state.boot_debounce = 0;
            false
        };

        let boot_just_pressed = boot_pressed && !self.button_state.boot_was_pressed;
        self.button_state.boot_was_pressed = boot_pressed;

        // Power key via AXP2101
        let pkey_pressed = {
            let mut i2c = axp2101.i2c.borrow_mut();
            let mut status = [0u8; 1];
            if i2c
                .write_read(AXP2101_ADDR, &[AXP2101_INTSTS2], &mut status)
                .is_ok()
            {
                let short_press = (status[0] & AXP2101_PKEY_SHORT_IRQ_BIT) != 0;
                if short_press {
                    let _ = i2c.write(AXP2101_ADDR, &[AXP2101_INTSTS2, 0xFF]);
                }
                short_press
            } else {
                false
            }
        };

        let pkey_just_pressed = pkey_pressed && !self.button_state.pkey_was_pressed;
        self.button_state.pkey_was_pressed = pkey_pressed;

        let any_pressed = boot_pressed || pkey_pressed;

        if self.state.game_over {
            if any_pressed {
                if self.button_state.hold_start_frame.is_none() {
                    self.button_state.hold_start_frame = Some(self.button_state.frame);
                } else if let Some(start) = self.button_state.hold_start_frame {
                    let hold_duration = self.button_state.frame.saturating_sub(start);
                    if hold_duration >= HOLD_TO_RESTART_FRAMES && !self.input.restart {
                        self.input.restart = true;
                        esp_println::println!("BTN: held long enough - restart!");
                    }
                }
            } else {
                self.button_state.hold_start_frame = None;
            }
        } else {
            self.button_state.hold_start_frame = None;

            if boot_just_pressed {
                self.input.turn_left = true;
                esp_println::println!("BTN: BOOT press -> turn left");
            }
            if pkey_just_pressed {
                self.input.turn_right = true;
                esp_println::println!("BTN: Power key press -> turn right");
            }
        }
    }

    pub fn handle_restart(&mut self) {
        if !self.input.restart {
            return;
        }

        esp_println::println!("=== RESTART TRIGGERED ===");
        esp_println::println!("  game_over was: {}", self.state.game_over);

        self.input.restart = false;
        let old_high_score_flag = self.state.new_high_score;
        self.state = GameState::default();
        self.state.new_high_score = old_high_score_flag;

        self.snake = Snake::new();
        self.food = None;
        self.state.needs_redraw = true;
    }

    pub fn process_turns(&mut self) {
        if self.input.restart {
            return;
        }
        if self.state.game_over {
            self.input.turn_left = false;
            self.input.turn_right = false;
            return;
        }

        if self.input.turn_left {
            self.snake.set_direction(self.snake.direction().turn_left());
            self.state.needs_redraw = true;
            self.input.turn_left = false;
        } else if self.input.turn_right {
            self.snake.set_direction(self.snake.direction().turn_right());
            self.state.needs_redraw = true;
            self.input.turn_right = false;
        }
    }

    pub fn animate_food(&mut self) {
        if let Some(food) = &mut self.food {
            food.food.animation_frame = food.food.animation_frame.wrapping_add(1);
        }
    }

    pub fn move_snake(&mut self) {
        if self.state.game_over {
            return;
        }
        self.snake.move_forward(&mut self.state);
    }

    pub fn check_collisions(&mut self) {
        if self.state.game_over {
            return;
        }

        if self.state.frames_elapsed < self.state.move_interval {
            return;
        }

        let head = self.snake.head();

        if head.x < 0 || head.x >= GRID_WIDTH || head.y < 0 || head.y >= GRID_HEIGHT {
            self.game_over("Wall collision", head);
            return;
        }

        for (idx, seg) in self.snake.segments().iter().enumerate() {
            if idx == 0 {
                continue; // ignore the segment that sits at old head position immediately after move
            }
            if *seg == head {
                self.game_over("Self collision", head);
                esp_println::println!("=== GAME OVER: Self collision with segment {} ===", idx);
                return;
            }
        }

        if let Some(food) = self.food {
            if food.position == head {
                let points = food.food.food_type.points();
                self.food = None;
                self.state.score += points;

                let tail_pos = self.snake.segments().last().copied().unwrap_or(head);
                let spawn_pos = if tail_pos == head {
                    match self.snake.direction() {
                        Direction::Up => Position {
                            x: head.x,
                            y: head.y + 1,
                        },
                        Direction::Down => Position {
                            x: head.x,
                            y: head.y - 1,
                        },
                        Direction::Left => Position {
                            x: head.x + 1,
                            y: head.y,
                        },
                        Direction::Right => Position {
                            x: head.x - 1,
                            y: head.y,
                        },
                    }
                } else {
                    tail_pos
                };

                self.snake.grow(spawn_pos);

                let speed_boost = match food.food.food_type {
                    FoodType::Regular => 1,
                    FoodType::Golden => 1,
                    FoodType::Special => 2,
                };
                for _ in 0..speed_boost {
                    if self.state.move_interval > 2 {
                        self.state.move_interval -= 1;
                    }
                }
                self.state.needs_redraw = true;

                esp_println::println!(
                    "Score: {} (+{} from {:?})",
                    self.state.score,
                    points,
                    food.food.food_type
                );
            }
        }
    }

    pub fn spawn_food_if_needed(&mut self) {
        if self.state.game_over {
            return;
        }
        if self.food.is_some() {
            return;
        }

        let mut buf = [0u8; 4];
        self.rng.read(&mut buf);
        let rand_val = (buf[0] as u32 | ((buf[1] as u32) << 8)) % 100;

        let food_type = if rand_val < FoodType::Regular.spawn_chance() {
            FoodType::Regular
        } else if rand_val < FoodType::Regular.spawn_chance() + FoodType::Golden.spawn_chance() {
            FoodType::Golden
        } else {
            FoodType::Special
        };

        let mut attempts = 0;
        while attempts < 100 {
            self.rng.read(&mut buf);
            let x = ((buf[0] as u32 | ((buf[1] as u32) << 8)) % GRID_WIDTH as u32) as i32;
            self.rng.read(&mut buf);
            let y = ((buf[0] as u32 | ((buf[1] as u32) << 8)) % GRID_HEIGHT as u32) as i32;

            let pos = Position { x, y };

            let occupied_snake = self.snake.segments().iter().any(|p| *p == pos);
            let occupied_head = self.snake.head() == pos;
            if !occupied_snake && !occupied_head {
                self.food = Some(FoodEntity {
                    position: pos,
                    food: Food {
                        food_type,
                        animation_frame: 0,
                    },
                });
                esp_println::println!("Food spawned: {:?} at ({}, {})", food_type, x, y);
                break;
            }
            attempts += 1;
        }
    }

    fn game_over(&mut self, reason: &str, head: Position) {
        self.state.game_over = true;
        self.state.needs_redraw = true;
        if self.high_score.update(self.state.score) {
            self.state.new_high_score = true;
        }
        self.dump_state(reason, head);
    }

    fn dump_state(&self, reason: &str, head_pos: Position) {
        let dir_str = match self.snake.direction() {
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
            self.state.score,
            self.state.frames_elapsed,
            self.state.move_interval
        );

        esp_println::println!("  Segments ({}):", self.snake.segments().len());
        for (idx, pos) in self.snake.segments().iter().enumerate() {
            esp_println::println!("    [{}]: ({}, {})", idx, pos.x, pos.y);
        }

        if let Some(food) = &self.food {
            esp_println::println!(
                "  Food: ({}, {}) type={:?}",
                food.position.x,
                food.position.y,
                food.food.food_type
            );
        } else {
            esp_println::println!("  Food: none");
        }
        esp_println::println!("=== END STATE DUMP ===");
    }

    pub fn should_render(&self) -> bool {
        self.state.needs_redraw
    }

    pub fn mark_rendered(&mut self) {
        self.state.needs_redraw = false;
    }

    pub fn head(&self) -> Position {
        self.snake.head()
    }

    pub fn segments(&self) -> &[Position] {
        self.snake.segments()
    }

    pub fn food(&self) -> Option<&FoodEntity> {
        self.food.as_ref()
    }
}
