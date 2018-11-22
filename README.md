Iconoscope
==========

Explore the icon database in your system.

![screenshot](data/screenshot.png)

Easily visualize icons to find inconsistencies, or check the look of icons in
different themes. Some useful features include:

 * Display icon information according to Freedesktop's
   [Icon Theme Specification][1] (size, scale, context, etc.).
 * Display icon file information (location, file size, image size, extension).
 * Compare the same icon across different installed themes.
 * Search all available icons by name.
 * Real time monitoring of an icon develompment folder (useful to catch
   inconcistencies at development time).

[1]: https://standards.freedesktop.org/icon-theme-spec/icon-theme-spec-latest.html

Compilation
-----------

Dependencies:
  * `gtk+-3.0`
  
In elementaryOS/Ubuntu install them with:

    sudo apt-get install libgtk-3-dev
    
Build with:

    ./pymk iconoscope
