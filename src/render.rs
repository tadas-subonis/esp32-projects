//! Reusable rendering primitives for grid-based games.
//!
//! This module provides generic framebuffer drawing utilities that can be
//! used across different games. It includes cell-based drawing, gradient
//! effects, animations, and text rendering helpers.

use core::fmt::Write;
use embedded_graphics::{
    mono_font::{ascii::FONT_10X20, MonoFont, MonoTextStyle},
    pixelcolor::Rgb888,
    prelude::*,
    primitives::{PrimitiveStyle, Rectangle},
    text::Text,
    Drawable,
};
use heapless::String;

use crate::config::{CELL_SIZE, GRID_OFFSET_X, GRID_OFFSET_Y, LCD_H_RES, LCD_V_RES};
use crate::display::MyFrameBuf;
use crate::game::{FoodEntity, Game, GameState, HighScore, Position};
use crate::hardware::DisplayDriver;

// =============================================================================
// Color Utilities
// =============================================================================

/// Apply brightness/intensity to a color.
///
/// Scales RGB values by intensity (0-255, where 255 = full brightness).
#[inline(always)]
pub fn apply_intensity(color: Rgb888, intensity: u8) -> Rgb888 {
    let r = ((color.r() as u16 * intensity as u16) / 255) as u8;
    let g = ((color.g() as u16 * intensity as u16) / 255) as u8;
    let b = ((color.b() as u16 * intensity as u16) / 255) as u8;
    Rgb888::new(r, g, b)
}

/// Calculate a pulsing brightness value for animations.
///
/// Returns a value between `min_brightness` and 255 based on the animation frame.
#[inline(always)]
pub fn pulse_brightness(animation_frame: u8, min_brightness: u8) -> u8 {
    let range = 255 - min_brightness;
    min_brightness + ((animation_frame.wrapping_mul(2) as u16 * range as u16) / 255) as u8
}

// =============================================================================
// Framebuffer Cell Drawing
// =============================================================================

/// Configuration for grid-based rendering.
#[derive(Clone, Copy)]
pub struct GridConfig {
    /// Width of the display in pixels
    pub display_width: usize,
    /// Height of the display in pixels
    pub display_height: usize,
    /// Size of each cell in pixels
    pub cell_size: usize,
    /// X offset for the grid (for centering)
    pub offset_x: i32,
    /// Y offset for the grid (for centering)
    pub offset_y: i32,
}

impl GridConfig {
    /// Create a new grid configuration.
    pub const fn new(
        display_width: usize,
        display_height: usize,
        cell_size: usize,
        offset_x: i32,
        offset_y: i32,
    ) -> Self {
        Self {
            display_width,
            display_height,
            cell_size,
            offset_x,
            offset_y,
        }
    }

    /// Convert grid coordinates to screen pixel coordinates.
    ///
    /// Returns None if the resulting position would be out of bounds.
    #[inline(always)]
    pub fn grid_to_screen(&self, grid_x: i32, grid_y: i32) -> Option<(usize, usize)> {
        let screen_x = self.offset_x + grid_x * self.cell_size as i32;
        let screen_y = self.offset_y + grid_y * self.cell_size as i32;

        if screen_x >= 0
            && screen_y >= 0
            && (screen_x as usize) + self.cell_size <= self.display_width
            && (screen_y as usize) + self.cell_size <= self.display_height
        {
            Some((screen_x as usize, screen_y as usize))
        } else {
            None
        }
    }
}

/// Fill a rectangular cell with a solid color.
///
/// This is the most basic drawing primitive for grid-based games.
#[inline(always)]
pub fn fill_cell(
    fb_data: &mut [Rgb888],
    x: usize,
    y: usize,
    color: Rgb888,
    cell_size: usize,
    row_width: usize,
    display_height: usize,
) {
    if x + cell_size <= row_width && y + cell_size <= display_height {
        for dy in 0..cell_size {
            let row_start = (y + dy) * row_width + x;
            for dx in 0..cell_size {
                fb_data[row_start + dx] = color;
            }
        }
    }
}

/// Fill a cell with a color adjusted by intensity (for gradients).
#[inline(always)]
pub fn fill_cell_gradient(
    fb_data: &mut [Rgb888],
    x: usize,
    y: usize,
    base_color: Rgb888,
    intensity: u8,
    cell_size: usize,
    row_width: usize,
    display_height: usize,
) {
    let color = apply_intensity(base_color, intensity);
    fill_cell(fb_data, x, y, color, cell_size, row_width, display_height);
}

/// Fill a cell with animated pulsing effect.
#[inline(always)]
pub fn fill_cell_animated(
    fb_data: &mut [Rgb888],
    x: usize,
    y: usize,
    base_color: Rgb888,
    animation_frame: u8,
    cell_size: usize,
    row_width: usize,
    display_height: usize,
) {
    let pulse = pulse_brightness(animation_frame, 128);
    let color = apply_intensity(base_color, pulse);
    fill_cell(fb_data, x, y, color, cell_size, row_width, display_height);
}

// =============================================================================
// Grid-Based Drawing with Config
// =============================================================================

/// Draw a cell at grid coordinates with a solid color.
#[inline(always)]
pub fn draw_grid_cell(
    fb_data: &mut [Rgb888],
    config: &GridConfig,
    grid_x: i32,
    grid_y: i32,
    color: Rgb888,
) {
    if let Some((x, y)) = config.grid_to_screen(grid_x, grid_y) {
        fill_cell(
            fb_data,
            x,
            y,
            color,
            config.cell_size,
            config.display_width,
            config.display_height,
        );
    }
}

/// Draw a cell at grid coordinates with gradient intensity.
#[inline(always)]
pub fn draw_grid_cell_gradient(
    fb_data: &mut [Rgb888],
    config: &GridConfig,
    grid_x: i32,
    grid_y: i32,
    base_color: Rgb888,
    intensity: u8,
) {
    if let Some((x, y)) = config.grid_to_screen(grid_x, grid_y) {
        fill_cell_gradient(
            fb_data,
            x,
            y,
            base_color,
            intensity,
            config.cell_size,
            config.display_width,
            config.display_height,
        );
    }
}

/// Draw a cell at grid coordinates with animation.
#[inline(always)]
pub fn draw_grid_cell_animated(
    fb_data: &mut [Rgb888],
    config: &GridConfig,
    grid_x: i32,
    grid_y: i32,
    base_color: Rgb888,
    animation_frame: u8,
) {
    if let Some((x, y)) = config.grid_to_screen(grid_x, grid_y) {
        fill_cell_animated(
            fb_data,
            x,
            y,
            base_color,
            animation_frame,
            config.cell_size,
            config.display_width,
            config.display_height,
        );
    }
}

// =============================================================================
// Text Rendering Utilities
// =============================================================================

/// Common colors for UI text.
pub mod colors {
    use embedded_graphics::pixelcolor::Rgb888;
    use embedded_graphics::prelude::RgbColor;

    pub const WHITE: Rgb888 = Rgb888::WHITE;
    pub const BLACK: Rgb888 = Rgb888::BLACK;
    pub const RED: Rgb888 = Rgb888::new(255, 0, 0);
    pub const GREEN: Rgb888 = Rgb888::new(0, 255, 0);
    pub const GOLD: Rgb888 = Rgb888::new(255, 215, 0);
    pub const GRAY: Rgb888 = Rgb888::new(128, 128, 128);
    pub const MAGENTA: Rgb888 = Rgb888::new(255, 0, 255);
}

/// Calculate the pixel width of text using a monospace font.
///
/// Assumes each character is `char_width` pixels wide.
#[inline]
pub fn text_width(text: &str, char_width: i32) -> i32 {
    text.len() as i32 * char_width
}

/// Calculate X position to center text horizontally on screen.
#[inline]
pub fn center_text_x(text: &str, char_width: i32, screen_width: i32) -> i32 {
    (screen_width - text_width(text, char_width)) / 2
}

/// Draw text at a specific position.
pub fn draw_text<D: DrawTarget<Color = Rgb888>>(
    target: &mut D,
    text: &str,
    x: i32,
    y: i32,
    color: Rgb888,
    font: &MonoFont,
) {
    Text::new(text, Point::new(x, y), MonoTextStyle::new(font, color))
        .draw(target)
        .ok();
}

/// Draw centered text.
pub fn draw_text_centered<D: DrawTarget<Color = Rgb888>>(
    target: &mut D,
    text: &str,
    y: i32,
    color: Rgb888,
    font: &MonoFont,
    screen_width: i32,
    char_width: i32,
) {
    let x = center_text_x(text, char_width, screen_width);
    draw_text(target, text, x, y, color, font);
}

/// Draw a formatted score string.
pub fn draw_score<D: DrawTarget<Color = Rgb888>>(
    target: &mut D,
    label: &str,
    score: u32,
    x: i32,
    y: i32,
    color: Rgb888,
) {
    let mut text = String::<32>::new();
    write!(text, "{}{}", label, score).ok();
    draw_text(target, text.as_str(), x, y, color, &FONT_10X20);
}

// =============================================================================
// Framebuffer to Display Transfer
// =============================================================================

/// Transfer framebuffer to display using run-length encoded row drawing.
///
/// This optimized transfer method groups identical pixels into rectangles,
/// reducing the number of draw calls significantly for sparse displays.
pub fn transfer_framebuffer_rle<D: DrawTarget<Color = Rgb888>>(
    display: &mut D,
    fb_data: &[Rgb888],
    width: usize,
    height: usize,
) {
    for y in 0..height {
        let row_start = y * width;
        let mut x = 0;
        while x < width {
            let pixel = fb_data[row_start + x];

            // Find run of identical pixels
            let start_x = x;
            let mut end_x = x + 1;
            while end_x < width && fb_data[row_start + end_x] == pixel {
                end_x += 1;
            }

            // Draw rectangle for this run
            let run_width = end_x - start_x;
            Rectangle::new(
                Point::new(start_x as i32, y as i32),
                Size::new(run_width as u32, 1),
            )
            .into_styled(PrimitiveStyle::with_fill(pixel))
            .draw(display)
            .ok();

            x = end_x;
        }
    }
}

// =============================================================================
// Default Grid Config for this hardware
// =============================================================================

/// Create the default grid configuration for the Waveshare display.
pub const fn default_grid_config() -> GridConfig {
    GridConfig::new(
        LCD_H_RES,
        LCD_V_RES,
        8, // cell size
        0, // offset x
        0, // offset y
    )
}

// =============================================================================
// Game rendering
// =============================================================================

/// Snake head color (darker green)
const SNAKE_HEAD_COLOR: Rgb888 = Rgb888::new(0, 200, 0);
/// Snake body base color (bright green)
const SNAKE_BODY_COLOR: Rgb888 = Rgb888::new(0, 255, 0);

fn render_food(fb_data: &mut [Rgb888], config: &GridConfig, food: Option<&FoodEntity>) {
    if let Some(food) = food {
        draw_grid_cell_animated(
            fb_data,
            config,
            food.position.x,
            food.position.y,
            food.food.food_type.color(),
            food.food.animation_frame,
        );
    }
}

fn render_snake_segments(fb_data: &mut [Rgb888], config: &GridConfig, segments: &[Position]) {
    let total_segments = segments.len().max(1);

    for (idx, pos) in segments.iter().enumerate() {
        // Intensity ranges from 255 (head-adjacent) to 128 (tail)
        let intensity = 128 + ((127 * (total_segments - idx)) / total_segments.max(1)) as u8;
        draw_grid_cell_gradient(fb_data, config, pos.x, pos.y, SNAKE_BODY_COLOR, intensity);
    }
}

fn render_snake_head(fb_data: &mut [Rgb888], config: &GridConfig, head: Position) {
    draw_grid_cell(fb_data, config, head.x, head.y, SNAKE_HEAD_COLOR);
}

fn render_game_over_ui(
    target: &mut MyFrameBuf,
    game_state: &GameState,
    high_score: &HighScore,
) {
    let game_over_text = if game_state.new_high_score {
        "NEW HIGH SCORE!"
    } else {
        "GAME OVER"
    };

    let center_y = LCD_V_RES as i32 / 2;

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

fn render_gameplay_ui(target: &mut MyFrameBuf, game_state: &GameState, high_score: &HighScore) {
    draw_score(
        target,
        "Score: ",
        game_state.score,
        8,
        LCD_V_RES as i32 - 40,
        colors::WHITE,
    );

    draw_score(
        target,
        "High: ",
        high_score.get(),
        8,
        LCD_V_RES as i32 - 20,
        colors::GOLD,
    );

    draw_text(
        target,
        "Boot=Left Pwr=Right",
        8,
        8,
        colors::GRAY,
        &FONT_10X20,
    );
}

/// Render the current frame to the framebuffer and flush to the display.
pub fn render_game(display: &mut DisplayDriver, fb: &mut MyFrameBuf, game: &Game) {
    fb.clear(Rgb888::BLACK).unwrap();

    let config = GridConfig::new(
        LCD_H_RES,
        LCD_V_RES,
        CELL_SIZE as usize,
        GRID_OFFSET_X,
        GRID_OFFSET_Y,
    );

    let fb_data: &mut [Rgb888] = &mut *fb.data;

    render_food(fb_data, &config, game.food());
    render_snake_segments(fb_data, &config, game.segments());
    render_snake_head(fb_data, &config, game.head());

    if game.state.game_over {
        render_game_over_ui(fb, &game.state, &game.high_score);
    } else {
        render_gameplay_ui(fb, &game.state, &game.high_score);
    }

    let fb_data: &[Rgb888] = &*fb.data;
    transfer_framebuffer_rle(display, fb_data, LCD_H_RES, LCD_V_RES);
    display.flush().ok();
}
