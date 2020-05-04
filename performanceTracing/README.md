# Media Foundation performance tracing
Some helpful information for capturing performance traces and diagnosing the underyling issues.

## How to capture a performance trace using WPR
1. Download 'MF_TRACE.WPRP' (Available in this repo) to your system under test.
2. From an **elevated** comand prompt run: `wpr.exe -start \path\to\MF_TRACE.WPRP!MediaPerformance -filemode`
3. Run your test scenario.
4. Return to your command prompt window and run `wpr.exe -stop mytrace.etl`

## Analyzing a Media Foundation Performance trace
In the future we will include some real world examples of performance investigations. For now we suggest you use Media Experience Analyzer (MXA) and information available online as well as in the [root of this repo](../README.md).

## Common Terminology
The media space is a very complicated one. So it comes as no surprise that it can prove difficult to communicate about the problems you are seeing.
This section provides some common terminology that we can all align on when investigating bugs for media scenarios on Windows.

### Video playback/performance
- Glitch: An overloaded and generic term which describes any scenario where a frame does not make it to the display at its intended time. 

- Stutter: Refers to a scenario where the framerate is visually incosistent. Typically caused by some component in the system (hardware or software) introducing a delay which causes a frame to not make it to the screen in time. It may appear that some frames move a little too fast. Some other ways you might describe stutter are 'jumpy' or 'not smooth'.

    - [Stutter example](./images/tears-stutter.gif)


- Out of order frame: A frame which is not displayed in its logical sequence. Imagine if you have 5 in order video frames: A, B, C, D, E. A, B, D, C, E : D was somehow displayed before C, instead of after. Visually this jump forward and then back will be very apparent.

    - [Out of order example](./images/tears-outoforder.gif?)


- Audio Video Sync (A/V Sync): The audio does not match the video. This scenario is most noticeable with talking heads. The audio may be ahead or behind where it should be. In some cases when this occurs the video playback speed may increase, similar to a 'fast-forward', to catch up with the audio.

- Flicker: The entire video window or the entire display flashes at an unexpected time. A relatively common example of this happening would be the entire display flickering black for a brief moment.

    - [Flicker example](./images/tears-flickerblack.gif)


- Corruption: Visual distortion in the video. Can be caused by a wide array of issue.

    - [Corruption (colorful) example](./images/corruption-colorful.gif)
    - [Corruption (dropped frames) example](./images/corruption-droppedframes.gif)


- Digital Rights Management (DRM): The technology used to protect premium digital content. Paid streaming services and digital rentals (such as Xbox movie rentals) use DRM to protect the content. This will either be Software-DRM (SW-DRM) or Hardware-DRM (HW-DRM).