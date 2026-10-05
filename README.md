# Summary

Canni is an open source personal finance tool. 
It's meant to help get preliminary estimates to financial questions to help focus sessions with a 
professional financial planner. You can think of it much like you would as an initial gut check or napkin math: 
it can be a decent start, but it shouldn't be treated as the end of the story. 

# Prerequisites 

Canni is built with `C++23`. Current build files use `cmake`. Canni optionally uses `gnuplot` for producing plots.

# Setup

**Prerequisites**: a C++23-capable compiler (Clang 16+ or GCC 13+), CMake 3.20+, and optionally gnuplot for plots.

On macOS with Homebrew:
```sh
brew install cmake gnuplot
```

**Build**:
```sh
git clone <repo-url> && cd canni
cmake -B build -DCMAKE_BUILD_TYPE=Release
cmake --build build
```

**Run the examples** (from the `build/` directory, so relative CSV paths resolve correctly):
```sh
cd build
./BasicPortfolio    # salary + housing scenario comparison
./OptionsPortfolio  # equity compensation with Monte Carlo
```

Each example prints a year-by-year tax summary and opens gnuplot windows for the scenario comparison and Monte Carlo plots. Close each window to continue to the next.

**Run the tests**:
```sh
cd build && ctest --output-on-failure
```

# Features

* Month-to-month financial projections based on event-driven financial simulation.
* Minimum retirement date estimates (`canni/tools/Retirement.h`).
* Plotted scenario comparisons ("Can I...?") (`canni/tools/Compare.h`).
* Monte Carlo simulation (`canni/tools/MonteCarlo.h`).

# Caveats

You should never make financial choices based solely on the output of this tool! 
While no one can truly predict the future, this tool in particular is provided as-is, and makes 
no guarantees on accuracy. Latent modeling bugs can potentially bias estimates significantly. 

The (human) author of this codebase occasionally uses human-supervised genAI when coding.
Parts of this codebase's source and test files were produced using Anthropic's Claude (Sonnet 4.6).
