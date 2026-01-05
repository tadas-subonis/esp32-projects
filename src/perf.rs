//! Performance tracking and metrics.
//!
//! Provides frame timing measurement and FPS tracking for games and applications.

/// Resource for tracking frame timing and performance metrics.
///
/// Use this to measure and log FPS, frame times, and identify performance issues.
///
/// # Example
/// ```ignore
/// // In your main loop:
/// let frame_start = embassy_time::Instant::now();
/// schedule.run(&mut world);
/// let frame_time_us = frame_start.elapsed().as_micros() as u64;
///
/// if let Some(mut perf) = world.get_resource_mut::<PerformanceMetrics>() {
///     perf.record_frame(frame_time_us);
///     if perf.should_log(300) {  // Log every 300 frames
///         perf.log_performance();
///     }
/// }
/// ```
pub struct PerformanceMetrics {
    /// Number of frames recorded since last reset
    pub frame_count: u32,
    /// Total frame time in microseconds since last reset
    pub total_frame_time_us: u64,
    /// Maximum frame time recorded since last reset
    pub max_frame_time_us: u64,
    /// Minimum frame time recorded since last reset
    pub min_frame_time_us: u64,
    /// Frame counter for logging interval tracking
    pub last_log_frame: u32,
    /// Whether to display FPS on screen (for future use)
    #[allow(dead_code)]
    pub show_on_screen: bool,
}

impl Default for PerformanceMetrics {
    fn default() -> Self {
        Self {
            frame_count: 0,
            total_frame_time_us: 0,
            max_frame_time_us: 0,
            min_frame_time_us: u64::MAX,
            last_log_frame: 0,
            show_on_screen: true,
        }
    }
}

impl PerformanceMetrics {
    /// Create a new PerformanceMetrics instance.
    pub fn new() -> Self {
        Self::default()
    }

    /// Record a frame's timing.
    ///
    /// Call this once per frame with the frame time in microseconds.
    pub fn record_frame(&mut self, frame_time_us: u64) {
        self.frame_count += 1;
        self.total_frame_time_us += frame_time_us;
        if frame_time_us > self.max_frame_time_us {
            self.max_frame_time_us = frame_time_us;
        }
        if frame_time_us < self.min_frame_time_us {
            self.min_frame_time_us = frame_time_us;
        }
    }

    /// Check if it's time to log performance (based on frame interval).
    ///
    /// Returns true if `last_log_frame` has exceeded the given interval.
    pub fn should_log(&mut self, interval: u32) -> bool {
        self.last_log_frame += 1;
        if self.last_log_frame >= interval {
            self.last_log_frame = 0;
            true
        } else {
            false
        }
    }

    /// Log performance metrics to serial output and reset counters.
    pub fn log_performance(&mut self) {
        if self.frame_count == 0 {
            return;
        }

        let avg_frame_time_us = self.total_frame_time_us / self.frame_count as u64;
        let avg_fps = if avg_frame_time_us > 0 {
            1_000_000 / avg_frame_time_us
        } else {
            0
        };

        esp_println::println!("=== Performance ({} frames) ===", self.frame_count);
        esp_println::println!("  Avg: {} us ({} FPS)", avg_frame_time_us, avg_fps);
        esp_println::println!("  Min: {} us", self.min_frame_time_us);
        esp_println::println!("  Max: {} us", self.max_frame_time_us);

        self.reset();
    }

    /// Reset all metrics counters.
    pub fn reset(&mut self) {
        self.frame_count = 0;
        self.total_frame_time_us = 0;
        self.max_frame_time_us = 0;
        self.min_frame_time_us = u64::MAX;
    }

    /// Get the current average FPS based on recorded frames.
    ///
    /// Returns 0 if no frames have been recorded.
    #[allow(dead_code)]
    pub fn get_current_fps(&self) -> u32 {
        if self.frame_count == 0 {
            return 0;
        }
        let avg_frame_time_us = self.total_frame_time_us / self.frame_count as u64;
        if avg_frame_time_us > 0 {
            (1_000_000 / avg_frame_time_us) as u32
        } else {
            0
        }
    }

    /// Get the average frame time in microseconds.
    ///
    /// Returns 0 if no frames have been recorded.
    #[allow(dead_code)]
    pub fn get_avg_frame_time_us(&self) -> u64 {
        if self.frame_count == 0 {
            0
        } else {
            self.total_frame_time_us / self.frame_count as u64
        }
    }

    /// Get the minimum frame time recorded (fastest frame).
    #[allow(dead_code)]
    pub fn get_min_frame_time_us(&self) -> u64 {
        if self.min_frame_time_us == u64::MAX {
            0
        } else {
            self.min_frame_time_us
        }
    }

    /// Get the maximum frame time recorded (slowest frame).
    #[allow(dead_code)]
    pub fn get_max_frame_time_us(&self) -> u64 {
        self.max_frame_time_us
    }
}
