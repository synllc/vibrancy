# `synllc/vibrancy`

Adds Mica and Acrylic rendering support to Electron browser windows. Currently, this addon only supports Windows 11 and above, with certain rendering modes only available on the most recent versions of the operating system. Bear in mind you **will** have to reimplement a title bar, as this solution removes the window's frame by default.

This module was written to replace [Glasstron](https://github.com/NyaomiDEV/Glasstron), which has been abandoned by its author. macOS and Linux support is planned to be added later on.

**NOTE:** If you are using a bundler like Webpack or Vite, then you **must** define `@synllc/vibrancy` as an external module in your build configuration because of its usage of native extensions.

## Getting started

### Prerequisites and installation

1. Make sure your project uses Electron 20 and above. Any older version will **not** work.
2. If not done already, install the tools necessary to compile native extensions with [`node-gyp`](https://github.com/nodejs/node-gyp). You can do this by opening the nodejs installer and selecting to install `node-gyp` tools in the process.
3. Install `synllc/vibrancy` with `pnpm add @synllc/vibrancy` (or equivalent command for your package manager).

### Setting up `electron/rebuild`

As the addon is not specifically precompiled for any version of Electron, you will need to setup [`electron/rebuild`](https://github.com/electron/rebuild) in your project's build process.

1. Install `electron/rebuild` with `pnpm add -D @electron/rebuild` (or equivalent command for your package manager).
2. Open your `project.json` configuration file and add the following rebuild script _if_ a rebuild process is not included in any other script already.

```json
"scripts": {
  "rebuild": ".\\node_modules\\.bin\\electron-rebuild"
}
```

3. Run `npm run rebuild` to build all `node-gyp` deps against your Electron version.

### Using `vibrancy` in your code.

```ts
import { VibrantBrowserWindow, Background, Theme, Corners, DwmSetting } from '@synllc/vibrancy'

function createWindow() {
  const window = new VibrantBrowserWindow({
    // Accepts all BrowserWindow constructor options.
    width: 800,
    height: 600,
    autoHideMenuBar: true,
    show: true,

    // Vibrancy-related options.
    effect: Background.Mica, // Can also use "TabbedMica" and "Acrylic".
    theme: Theme.Dark, // Set to "Theme.Auto" for system-dependent theming.
  })

  // You can update effects at any time.
  window.setVisualEffect(Background.Acrylic, Theme.Light)

  // You can also adjust the roundness of the window's corners!
  window.setVisualEffect(DwmSetting.Corner, Corners.SlightlyRound)
}
```
