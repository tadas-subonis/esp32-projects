//! Hardware driver support.
//!
//! Contains TCA9554 reset interface, AXP2101 PMU support, and hardware resources.

use alloc::rc::Rc;
use core::cell::RefCell;
use esp_hal::delay::Delay;
use esp_hal::gpio::Input;
use esp_hal::i2c::master::I2c;
use sh8601_rs::{ResetInterface, Sh8601Driver, Ws18AmoledDriver};

use crate::config::{TCA9554_CONFIG, TCA9554_OUTPUT, TCA9554_POLARITY};

/// Shared TCA9554 reset interface that uses Rc to share I2C bus.
///
/// The TCA9554 I/O expander controls the display reset and power lines.
pub struct SharedTca9554Reset {
    pub i2c: Rc<RefCell<I2c<'static, esp_hal::Blocking>>>,
    pub addr: u8,
}

impl ResetInterface for SharedTca9554Reset {
    type Error = <I2c<'static, esp_hal::Blocking> as embedded_hal::i2c::ErrorType>::Error;

    fn reset(&mut self) -> Result<(), Self::Error> {
        let delay = Delay::new();
        let mut i2c = self.i2c.borrow_mut();
        i2c.write(self.addr, &[TCA9554_CONFIG, 0b0111_1000])?;
        i2c.write(self.addr, &[TCA9554_POLARITY, 0x00])?;
        i2c.write(self.addr, &[TCA9554_OUTPUT, 0b1000_0110])?;
        delay.delay_millis(20);
        i2c.write(self.addr, &[TCA9554_OUTPUT, 0b1000_0111])?;
        delay.delay_millis(150);
        Ok(())
    }
}

/// AXP2101 PMU resource for reading power button.
pub struct Axp2101Resource {
    pub i2c: Rc<RefCell<I2c<'static, esp_hal::Blocking>>>,
}

/// Type alias for the display driver to simplify the type.
pub type DisplayDriver = Sh8601Driver<Ws18AmoledDriver, SharedTca9554Reset>;

/// Display resource - NonSend because it contains non-thread-safe components.
pub struct DisplayResource {
    pub display: DisplayDriver,
}

/// Button resources - NonSend because GPIO pins are not Send.
pub struct ButtonLeftResource {
    pub button: Input<'static>,
}
