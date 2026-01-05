//! Minimal fixed-function game loop for the snake game.
//!
//! Owns core resources (game state, framebuffer, perf metrics) and runs the
//! deterministic update → render → blit pipeline.

use crate::display::FrameBufferResource;
use crate::game::Game;
use crate::hardware::{Axp2101Resource, ButtonLeftResource, DisplayDriver};
use crate::perf::PerformanceMetrics;
use crate::render::render_game;

pub struct Engine {
    pub game: Game,
    pub framebuffer: FrameBufferResource,
    pub perf: PerformanceMetrics,
}

impl Engine {
    pub fn new(game: Game, framebuffer: FrameBufferResource, perf: PerformanceMetrics) -> Self {
        Self {
            game,
            framebuffer,
            perf,
        }
    }

    /// Run one frame of input → simulation → render (if needed).
    pub fn run_frame(
        &mut self,
        display: &mut DisplayDriver,
        buttons: &ButtonLeftResource,
        axp2101: &mut Axp2101Resource,
    ) {
        self.game.poll_inputs(buttons, axp2101);
        self.game.handle_restart();
        self.game.process_turns();
        self.game.animate_food();
        self.game.move_snake();
        self.game.check_collisions();
        self.game.spawn_food_if_needed();

        if self.game.should_render() {
            render_game(display, &mut self.framebuffer.frame_buf, &self.game);
            self.game.mark_rendered();
        }
    }

    pub fn record_frame_time(&mut self, frame_time_us: u64) {
        self.perf.record_frame(frame_time_us);
    }
}
