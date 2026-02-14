"""Hardware Input Module
----------------------
Kablolu kumanda (butonlar) okuma ve debounce.

Butonlar:
    FORWARD, BACK, LEFT, RIGHT, FIRE, MODE_SWITCH

GPIO Kütüphanesi:
    RPi.GPIO tercih; yoksa mock mod (herhangi bir gerçek input gelmez, manuel test için set_state ile).

Arayüz:
    class GamepadGPIO:
        - read() -> dict{"forward":bool,...} anlık durum
        - register_callback(name, func) : durum değişiminde tetiklenecek (edge)
        - update_poll() : thread içinde periyodik çağrılır
        - mock_set(name, value) : test için.

Debounce:
    Her buton için son değişim zamanını izleyip min_interval (default 0.1s) altında ikinci değişimi yoksayar.
"""
from __future__ import annotations
import time
from typing import Callable, Dict

try:
    import RPi.GPIO as GPIO  # type: ignore
except Exception:
    GPIO = None  # type: ignore


BUTTON_PINS = {
    'forward': 5,
    'back': 6,
    'left': 13,
    'right': 19,
    'fire': 26,
    'mode': 21,
}


class GamepadGPIO:
    def __init__(self, debounce_interval: float = 0.1, use_pullup: bool = True):
        self.debounce_interval = debounce_interval
        self.use_pullup = use_pullup
        self.available = GPIO is not None
        self.last_change: Dict[str, float] = {k: 0.0 for k in BUTTON_PINS}
        self.state: Dict[str, bool] = {k: False for k in BUTTON_PINS}
        self.callbacks: Dict[str, Callable[[bool], None]] = {}
        if self.available:
            self._setup_gpio()
        else:
            print('[GPIO] Kütüphane yok, mock mod')

    def _setup_gpio(self):
        try:
            GPIO.setmode(GPIO.BCM)
            for name, pin in BUTTON_PINS.items():
                if self.use_pullup:
                    GPIO.setup(pin, GPIO.IN, pull_up_down=GPIO.PUD_UP)
                else:
                    GPIO.setup(pin, GPIO.IN)
            print('[GPIO] Butonlar ayarlandı')
        except Exception as e:
            print('[GPIO] Setup hata, mock moda düşülüyor:', e)
            self.available = False

    def register_callback(self, name: str, func: Callable[[bool], None]):
        if name in BUTTON_PINS:
            self.callbacks[name] = func

    def read(self) -> Dict[str, bool]:
        if not self.available:
            return self.state.copy()
        current = {}
        for name, pin in BUTTON_PINS.items():
            val = GPIO.input(pin)
            # pull-up kullanılıyorsa butona basıldığında 0 okunur, tersle
            pressed = (val == 0) if self.use_pullup else (val == 1)
            current[name] = pressed
        return current

    def update_poll(self):
        """Periyodik olarak çağrılır; değişiklikleri tespit eder ve debounce uygular."""
        current = self.read()
        now = time.time()
        for name, pressed in current.items():
            prev = self.state[name]
            if pressed != prev:
                if now - self.last_change[name] >= self.debounce_interval:
                    self.state[name] = pressed
                    self.last_change[name] = now
                    cb = self.callbacks.get(name)
                    if cb:
                        try:
                            cb(pressed)
                        except Exception:
                            pass

    # Mock fonksiyonlar
    def mock_set(self, name: str, value: bool):
        if name in self.state:
            self.state[name] = value
            cb = self.callbacks.get(name)
            if cb:
                cb(value)

    def cleanup(self):
        if self.available:
            try:
                GPIO.cleanup()
            except Exception:
                pass


__all__ = ["GamepadGPIO", "BUTTON_PINS"]
