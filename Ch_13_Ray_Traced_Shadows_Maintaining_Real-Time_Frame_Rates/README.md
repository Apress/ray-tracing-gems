This project contains a source code accompanying the article "Ray Traced Shadows: Maintaining Real-Time Frame Rates", by Jakub Boksansky, Michael Wimmer, and Jiri Bittner which appears in the [Ray Tracing Gems](http://www.raytracinggems.com) book.

As the source code depends on the [Falcor 3.2 framework](https://github.com/NVIDIAGameWorks/Falcor), you need to add Falcor 3.2 to the directory of this project. Follow these steps:
1. Clone [Falcor 3.2](https://github.com/NVIDIAGameWorks/Falcor) and then move or copy that `Falcor` directory into this chapter's directory, i.e., `Falcor` should be in the same directory as `dxrShadows`, `Media`, and `dxrShadows.sln`
2. Open `dxrShadows.sln`
3. Compile, link, and run

Build note: Make sure to place this whole project into a path that does not contain spaces to prevent build errors.
