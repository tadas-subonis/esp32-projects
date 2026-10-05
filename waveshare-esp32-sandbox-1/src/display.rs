//! Display and framebuffer support.
//!
//! Contains the heap-backed framebuffer implementation for the SH8601 AMOLED display.

use alloc::boxed::Box;
use embedded_graphics::pixelcolor::{PixelColor, Rgb888};
use embedded_graphics::prelude::RgbColor;
use embedded_graphics_framebuf::backends::FrameBufferBackend;
use embedded_graphics_framebuf::FrameBuf;

use crate::config::{LCD_BUFFER_SIZE, LCD_H_RES, LCD_V_RES};

/// Heap-allocated buffer for framebuffer storage.
///
/// This allows allocating the large framebuffer on PSRAM instead of stack.
pub struct HeapBuffer<C: PixelColor, const N: usize>(Box<[C; N]>);

impl<C: PixelColor, const N: usize> HeapBuffer<C, N> {
    pub fn new(data: Box<[C; N]>) -> Self {
        Self(data)
    }
}

impl<C: PixelColor, const N: usize> core::ops::Deref for HeapBuffer<C, N> {
    type Target = [C; N];
    fn deref(&self) -> &Self::Target {
        &self.0
    }
}

impl<C: PixelColor, const N: usize> core::ops::DerefMut for HeapBuffer<C, N> {
    fn deref_mut(&mut self) -> &mut Self::Target {
        &mut self.0
    }
}

impl<C: PixelColor, const N: usize> FrameBufferBackend for HeapBuffer<C, N> {
    type Color = C;
    fn set(&mut self, index: usize, color: Self::Color) {
        self.0[index] = color;
    }
    fn get(&self, index: usize) -> Self::Color {
        self.0[index]
    }
    fn nr_elements(&self) -> usize {
        N
    }
}

/// Type alias for the framebuffer backing store.
pub type FbBuffer = HeapBuffer<Rgb888, LCD_BUFFER_SIZE>;

/// Type alias for the complete framebuffer type.
pub type MyFrameBuf = FrameBuf<Rgb888, FbBuffer>;

/// Framebuffer wrapper.
pub struct FrameBufferResource {
    pub frame_buf: MyFrameBuf,
}

impl FrameBufferResource {
    pub fn new() -> Self {
        let fb_data: Box<[Rgb888; LCD_BUFFER_SIZE]> = Box::new([Rgb888::BLACK; LCD_BUFFER_SIZE]);
        let heap_buffer = HeapBuffer::new(fb_data);
        let frame_buf = MyFrameBuf::new(heap_buffer, LCD_H_RES, LCD_V_RES);
        Self { frame_buf }
    }
}

impl Default for FrameBufferResource {
    fn default() -> Self {
        Self::new()
    }
}
