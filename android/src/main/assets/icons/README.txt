ForgeEngine Icon Assets
=======================

Place SVG icons in svg/ and PNG fallbacks in png/.

See FORGE_BUILD_SPEC.txt §8 for the complete list of 70 icon filenames.

The engine automatically generates minimal SVG fallbacks at runtime
if icon files are missing, so the app will work without any icons here.

Recommended SVG format:
  <svg xmlns="http://www.w3.org/2000/svg" viewBox="0 0 24 24" width="24" height="24">
    <path fill="white" d="..."/>
  </svg>

Use white fill (#ffffff) — the C++ IconSystem tints icons at render time.
