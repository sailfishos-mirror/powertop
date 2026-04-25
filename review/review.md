# PowerTOP review guidelines

PowerTOP is a Linux CLI application used to diagnose software that is
causing high power usage, and to suggest tunings on the system for best
power.


## Review Process

A three step process
1. Gather context
2. Perform code review
3. Report results


### Gather context

When doing code review, always gather the whole function under review,
even if a PR or patch only touches a few lines.
For patch/PR reviews, collect both the original and the patched full
function.

In addition, load these files into the context:
- `style.md` - Coding style guide
- `general-c++.md` - C++ coding rules

