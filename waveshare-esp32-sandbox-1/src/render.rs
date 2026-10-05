//! Reusable rendering primitives for grid-based games.
//!
//! This module provides generic framebuffer drawing utilities that can be
//! used across different games. It includes cell-based drawing, gradient
//! effects, animations, and text rendering helpers.

use core::fmt::Write;
use embedded_graphics::{
    mono_font::{ascii::FONT_10X20, ascii::FONT_6X9, MonoFont, MonoTextStyle},
    pixelcolor::Rgb888,
    prelude::*,
    primitives::{PrimitiveStyle, Rectangle},
    text::Text,
    Drawable,
};
use heapless::String;

use crate::config::{
    CELL_SIZE, GRID_OFFSET_X, GRID_OFFSET_Y, LCD_H_RES, LCD_V_RES, UI_PADDING_X, UI_PADDING_Y,
    UI_TEXT_SCALE_DEN, UI_TEXT_SCALE_NUM,
};
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

struct ScaledDrawTarget<'a, D> {
    target: &'a mut D,
    origin: Point,
    scale_num: i32,
    scale_den: i32,
}

impl<'a, D> ScaledDrawTarget<'a, D> {
    fn new(target: &'a mut D, origin: Point, scale_num: i32, scale_den: i32) -> Self {
        Self {
            target,
            origin,
            scale_num,
            scale_den,
        }
    }
}

impl<D: DrawTarget<Color = Rgb888> + OriginDimensions> DrawTarget for ScaledDrawTarget<'_, D> {
    type Color = Rgb888;
    type Error = D::Error;

    fn draw_iter<I>(&mut self, pixels: I) -> Result<(), Self::Error>
    where
        I: IntoIterator<Item = Pixel<Self::Color>>,
    {
        for Pixel(point, color) in pixels {
            let x0 = self.origin.x + (point.x * self.scale_num) / self.scale_den;
            let y0 = self.origin.y + (point.y * self.scale_num) / self.scale_den;
            let x1 = self.origin.x + ((point.x + 1) * self.scale_num) / self.scale_den;
            let y1 = self.origin.y + ((point.y + 1) * self.scale_num) / self.scale_den;
            let width = (x1 - x0).max(1) as u32;
            let height = (y1 - y0).max(1) as u32;
            Rectangle::new(Point::new(x0, y0), Size::new(width, height))
                .into_styled(PrimitiveStyle::with_fill(color))
                .draw(self.target)?;
        }
        Ok(())
    }
}

impl<D: OriginDimensions> OriginDimensions for ScaledDrawTarget<'_, D> {
    fn size(&self) -> Size {
        self.target.size()
    }
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

/// Draw scaled text at a specific position.
pub fn draw_text_scaled<D: DrawTarget<Color = Rgb888> + OriginDimensions>(
    target: &mut D,
    text: &str,
    x: i32,
    y: i32,
    color: Rgb888,
    font: &MonoFont,
    scale_num: i32,
    scale_den: i32,
) {
    if scale_num <= scale_den {
        draw_text(target, text, x, y, color, font);
        return;
    }

    let mut scaled_target =
        ScaledDrawTarget::new(target, Point::new(x, y), scale_num, scale_den);
    Text::new(text, Point::zero(), MonoTextStyle::new(font, color))
        .draw(&mut scaled_target)
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

/// Draw centered scaled text.
pub fn draw_text_centered_scaled<D: DrawTarget<Color = Rgb888> + OriginDimensions>(
    target: &mut D,
    text: &str,
    y: i32,
    color: Rgb888,
    font: &MonoFont,
    screen_width: i32,
    scale_num: i32,
    scale_den: i32,
) {
    let char_width = font.character_size.width as i32 * scale_num / scale_den;
    let x = center_text_x(text, char_width, screen_width);
    draw_text_scaled(target, text, x, y, color, font, scale_num, scale_den);
}

/// Draw a formatted score string.
pub fn draw_score_scaled<D: DrawTarget<Color = Rgb888> + OriginDimensions>(
    target: &mut D,
    label: &str,
    score: u32,
    x: i32,
    y: i32,
    color: Rgb888,
    scale_num: i32,
    scale_den: i32,
) {
    let mut text = String::<32>::new();
    write!(text, "{}{}", label, score).ok();
    draw_text_scaled(
        target,
        text.as_str(),
        x,
        y,
        color,
        &FONT_10X20,
        scale_num,
        scale_den,
    );
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
const SNAKE_HEAD_COLOR: Rgb888 = Rgb888::new(0, 160, 0);
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
    let line_height =
        FONT_10X20.character_size.height as i32 * UI_TEXT_SCALE_NUM / UI_TEXT_SCALE_DEN;
    let title_y = center_y - (line_height * 2);
    let score_y = center_y - line_height;
    let high_score_y = center_y;
    let hint_y = center_y + line_height;

    draw_text_centered_scaled(
        target,
        game_over_text,
        title_y,
        title_color,
        &FONT_10X20,
        LCD_H_RES as i32,
        UI_TEXT_SCALE_NUM,
        UI_TEXT_SCALE_DEN,
    );

    let mut score_text = String::<20>::new();
    write!(score_text, "Score: {}", game_state.score).ok();
    draw_text_centered_scaled(
        target,
        score_text.as_str(),
        score_y,
        colors::WHITE,
        &FONT_10X20,
        LCD_H_RES as i32,
        UI_TEXT_SCALE_NUM,
        UI_TEXT_SCALE_DEN,
    );

    let mut high_score_text = String::<30>::new();
    write!(high_score_text, "High: {}", high_score.get()).ok();
    draw_text_centered_scaled(
        target,
        high_score_text.as_str(),
        high_score_y,
        colors::GOLD,
        &FONT_10X20,
        LCD_H_RES as i32,
        UI_TEXT_SCALE_NUM,
        UI_TEXT_SCALE_DEN,
    );

    draw_text_centered_scaled(
        target,
        "Hold both to restart",
        hint_y,
        colors::GRAY,
        &FONT_10X20,
        LCD_H_RES as i32,
        UI_TEXT_SCALE_NUM,
        UI_TEXT_SCALE_DEN,
    );
}

fn render_gameplay_ui(target: &mut MyFrameBuf, game_state: &GameState, high_score: &HighScore) {
    let line_height =
        FONT_10X20.character_size.height as i32 * UI_TEXT_SCALE_NUM / UI_TEXT_SCALE_DEN;
    let bottom_y = LCD_V_RES as i32 - UI_PADDING_Y - line_height;
    let above_bottom_y = bottom_y - line_height;

    draw_score_scaled(
        target,
        "Score: ",
        game_state.score,
        UI_PADDING_X,
        above_bottom_y,
        colors::WHITE,
        UI_TEXT_SCALE_NUM,
        UI_TEXT_SCALE_DEN,
    );

    draw_score_scaled(
        target,
        "High: ",
        high_score.get(),
        UI_PADDING_X,
        bottom_y,
        colors::GOLD,
        UI_TEXT_SCALE_NUM,
        UI_TEXT_SCALE_DEN,
    );

    draw_text_scaled(
        target,
        "Boot=Left Pwr=Right",
        UI_PADDING_X,
        UI_PADDING_Y,
        colors::GRAY,
        &FONT_10X20,
        UI_TEXT_SCALE_NUM,
        UI_TEXT_SCALE_DEN,
    );
}

fn render_status_overlay(target: &mut MyFrameBuf, fps: u32, battery_percent: Option<u8>) {
    let font = &FONT_6X9;
    let char_width = font.character_size.width as i32;
    let line_height = font.character_size.height as i32 + 2;

    let mut fps_text = String::<16>::new();
    write!(fps_text, "FPS: {}", fps).ok();
    let fps_x = LCD_H_RES as i32 - UI_PADDING_X - text_width(fps_text.as_str(), char_width);
    let fps_y = UI_PADDING_Y;
    draw_text(
        target,
        fps_text.as_str(),
        fps_x,
        fps_y,
        colors::GRAY,
        font,
    );

    let mut batt_text = String::<16>::new();
    if let Some(percent) = battery_percent {
        write!(batt_text, "BAT: {}%", percent).ok();
    } else {
        batt_text.push_str("BAT: --").ok();
    }
    let batt_x = LCD_H_RES as i32 - UI_PADDING_X - text_width(batt_text.as_str(), char_width);
    let batt_y = fps_y + line_height;
    draw_text(
        target,
        batt_text.as_str(),
        batt_x,
        batt_y,
        colors::GRAY,
        font,
    );
}

/// Render the current frame to the framebuffer and flush to the display.
pub fn render_game(
    display: &mut DisplayDriver,
    fb: &mut MyFrameBuf,
    game: &Game,
    fps: u32,
    battery_percent: Option<u8>,
) {
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
    render_status_overlay(fb, fps, battery_percent);

    let fb_data: &[Rgb888] = &*fb.data;
    transfer_framebuffer_rle(display, fb_data, LCD_H_RES, LCD_V_RES);
    display.flush().ok();
}
