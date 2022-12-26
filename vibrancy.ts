// JS implementation of support for our mica gyp.

import { BrowserWindow, BrowserWindowConstructorOptions } from 'electron'
const { submitDwmCommand, redrawWindow } = require(__dirname + '/addon/build/Release/hwvibrancy.node')

function removeFrame(window: BrowserWindow) {
  const hwnd = window.getNativeWindowHandle().readInt32LE()
  const bounds = window.getBounds()
  window.hide()
  redrawWindow(hwnd, bounds.x, bounds.y, bounds.width, bounds.height)
  window.show()
}

export const enum Background {
  Automatic,
  None,
  Mica,
  Acrylic,
  TabbedMica,
}

export const enum DwmSetting {
  Corner = 5,
  BorderColor,
  CaptionColor,
  TextColor,
  Frame,
}

export const enum Theme {
  Auto,
  Dark,
  Light,
}

export const enum Corners {
  Default,
  Square,
  Round,
  SlightlyRound,
}

export type VibrantBrowserWindowConstructorOptions = Omit<BrowserWindowConstructorOptions, 'transparent' | 'backgroundColor'> & {
  effect?: Background | DwmSetting
  theme?: Theme | Corners
}

export class VibrantBrowserWindow extends BrowserWindow {
  private _lastTheme: VibrantBrowserWindowConstructorOptions['theme']
  private _lastEffect: VibrantBrowserWindowConstructorOptions['effect']

  setVisualEffect(params: VibrantBrowserWindowConstructorOptions['effect'], value: VibrantBrowserWindowConstructorOptions['theme']) {
    const hwnd = this.getNativeWindowHandle().readInt32LE()
    submitDwmCommand(hwnd, params!.valueOf(), value!.valueOf())
    this._lastEffect = params
    this._lastTheme = value
  }

  constructor(options: VibrantBrowserWindowConstructorOptions) {
    super({
      ...options,
      transparent: false,
      frame: false,
      backgroundColor: '#000000ff',
    })
    ;(this._lastEffect = options.effect ?? Background.Mica), (this._lastTheme = options.theme ?? Theme.Auto)

    let interval: NodeJS.Timer | undefined
    let frameHasBeenRemoved = false
    const applyEffect = () => {
      this.setVisualEffect(this._lastEffect, this._lastTheme)
    }

    this.on('show', () => {
      if (!frameHasBeenRemoved) {
        frameHasBeenRemoved = true // To prevent an inf loop.
        interval = setInterval(applyEffect, 60)
        removeFrame(this)
      }
      applyEffect()
    })

    this.on('closed', () => interval !== undefined && clearInterval(interval))
  }
}
