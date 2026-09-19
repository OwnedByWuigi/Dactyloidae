## libgopher

libgopher is a fork of OverbiteFF, a plugin which lets you browse Gopher sites on legacy Firefox-based browsers such as Dactyloidae.

In this tree it is built into the browser rather than installed as an add-on. The browser registers the Gopher protocol handler during startup, so `gopher://` links work immediately in a new profile.

### Built-in integration

The integration is split into the same pieces used by other browser-native features:

- `moz.build` adds the component files, default preferences, and chrome package to the browser build.
- `components/protocol.manifest` registers the Gopher protocol, while `components/about.manifest` registers the `about:gopher` and `about:overbite` pages.
- `components/protocol.js` implements Gopher networking, directory conversion, internal resources, and supported item types.
- `components/about.js` supplies the Overbite information page.
- `jar.mn` packages the `overbiteff` content and locale namespaces.
- `defaults/preferences/overbiteff.js` initializes the build marker preference.

The browser-level build entry is `browser/moz.build`; no XPI installation or add-on manager registration is required.

### Supported URLs

The primary entry point is any Gopher URL, for example:

```text
gopher://gopher.floodgap.com/
```

The built-in information pages are available at:

```text
about:gopher
about:overbite
```

The protocol handler also serves the generated internal resources used by Gopher directory pages, including icons, CSS, and the page interaction script. These resources are addressed through `gopher:///internal-*` URLs and are resolved from the packaged `content/chrome/` directory.

### Gopher item types

Directory conversion includes the traditional Gopher item types for text, menus, CSO/ph, errors, archives, searches, Telnet, images, sound, HTML, movies, PNG, PDF, and generic binary content. Unknown types receive the generic icon and label instead of preventing the directory from loading.

### Profile customization

The protocol handler first checks the profile’s `gopherchrome` directory for replacements for internal CSS, JavaScript, and image resources. This allows local visual or behavior changes without editing the browser package.

The following preferences are recognized when present:

- `extensions.overbiteff.dotless` — enables dotless text cleanup.
- `extensions.overbiteff.fixitype` — enables the legacy item-type correction behavior.
- `extensions.overbiteff.buildmark` — records the initialized OverbiteFF build number.

Only the build marker is given a default value by the built-in preference file; the other options remain opt-in.
