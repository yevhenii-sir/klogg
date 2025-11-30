# Wrap Text Analysis and Implementation Details

## Overview
This document analyzes the implementation of "Wrap text" in Klogg, focusing on the `AbstractLogView` and its derived classes. It details the mechanism for displaying wrapped lines, calculating scroll ranges, and handling bottom alignment, which has been a source of bugs (e.g., last line visibility).

## Windows Displaying File Content

Klogg has two main views that display file content:

### 1. LogMainView (Top View)
- **File**: `src/ui/src/logmainview.cpp`
- **Purpose**: Displays the complete log file
- **Inherits**: `AbstractLogView`
- **Features**:
  - Shows all lines from the original file
  - Has an Overview widget showing matched lines
  - Line numbers correspond to actual file line numbers

### 2. FilteredView (Bottom View)
- **File**: `src/ui/src/filteredview.cpp`  
- **Purpose**: Displays filtered/searched results
- **Inherits**: `AbstractLogView`
- **Features**:
  - Shows only matched lines from search
  - Line numbers show original file line numbers
  - Supports visibility filters (Marks, Matches, or both)

Both views share the text wrapping implementation from `AbstractLogView`.

## Implementation Details

### 1. Core Classes

#### AbstractLogView (`src/ui/include/abstractlogview.h`)
The base class that handles all rendering and text wrapping logic.

Key members:
```cpp
bool useTextWrap_ = false;           // Whether text wrapping is enabled
bool lastLineAligned_ = false;       // Whether view is bottom-aligned
LineNumber firstLine_;               // First visible line
TextAreaCache textAreaCache_;        // Cached rendering with actual_height_
```

#### WrappedString (`src/ui/include/wrappedstring.h`)
Handles text wrapping logic by splitting long lines at word boundaries or column limits.

```cpp
class WrappedString {
    // Splits text into wrapped parts based on visible columns
    explicit WrappedString(QString longLine, LineLength visibleColumns);
    size_t wrappedLinesCount() const;      // Number of visual lines
    WrappedStringPart wrappedLine(size_t index) const;  // Get specific visual line
};
```

### 2. Key Functions

#### `getNbBottomWrappedVisibleLines()` - Calculate Wrapped Line Count
- **File**: `src/ui/src/abstractlogview.cpp`
- **Purpose**: Calculates how many *unwrapped* (logical) lines fit in the viewport when text wrapping is enabled
- **Logic**:
  1. Iterates backwards from the last line of the log
  2. Uses `WrappedString` to simulate wrapping of each line based on `getNbVisibleCols()`
  3. Accumulates wrapped line counts until they fill `getNbVisibleLines()` (viewport height in characters)
  4. Returns the count of *logical* lines that fit in the viewport when wrapped

**Logging Tag**: `[TextWrap:Calc]`

#### `updateScrollBars()` - Set Scroll Range
- **Purpose**: Sets the vertical scrollbar range
- **Logic**:
  - Without wrap: Range is `[0, TotalLines - VisibleLines]`
  - With wrap: Range is `[0, TotalLines - UnwrappedLinesAtBottom]`
  - The maximum scroll value corresponds to the first line of the "last page" of content

**Logging Tag**: `[TextWrap:ScrollBar]`

#### `scrollContentsBy()` - Handle Scroll Events
- **Purpose**: Updates view position and bottom alignment state
- **Logic**:
  1. Calculates `lastTopLine` (first line when scrolled to bottom)
  2. If `scrollPosition >= lastTopLine`, enters "Bottom Alignment" mode
  3. Sets `lastLineAligned_ = true` and anchors `firstLine_` appropriately

**Logging Tag**: `[TextWrap:Scroll]`

#### `paintEvent()` - Render View
- **Purpose**: Renders the text area with proper offset for bottom alignment
- **Logic**:
  - If `lastLineAligned_` is true:
    - Uses `textAreaCache_.actual_height_` for wrapped text
    - Calculates `drawingTopOffset_` to align bottom of content with viewport bottom
    - Shifts painting position accordingly

**Logging Tag**: `[TextWrap:Paint]`

#### `updateDisplaySize()` - Handle Resize
- **Purpose**: Updates view dimensions and scroll state on resize
- **Logic**:
  1. Updates character dimensions
  2. Updates scroll bars
  3. Invalidates cache
  4. For text wrap: Creates null pixmap (lazy initialization)
  5. If was bottom-aligned: Restores bottom alignment after resize

**Logging Tag**: `[TextWrap:Resize]`

#### `drawTextArea()` - Draw Content
- **Purpose**: Renders text content to pixmap
- **Logic**:
  1. Creates dynamic pixmap for wrapped content (can't pre-calculate height)
  2. Iterates through lines, wrapping each one
  3. For bottom-aligned views: Continues drawing past viewport to ensure complete rendering
  4. Stores `actual_height_` for `paintEvent()` to use

**Logging Tag**: `[TextWrap:Draw]`

### 3. Bottom Alignment Mechanism ("Lock" to Bottom)

**Goal**: When at the bottom of the log, ensure the last line is flush with the bottom of the viewport.

**Detection Flow**:
1. In `scrollContentsBy()`: If `scrollPosition >= lastTopLine`, enter bottom alignment
2. Set `lastLineAligned_ = true`
3. Set `firstLine_` to the calculated `lastTopLine`

**Rendering Flow**:
1. In `paintEvent()`: Check `lastLineAligned_`
2. Use `actual_height_` (actual pixel height of drawn content)
3. Calculate `drawingTopOffset_ = -(effectiveHeight - viewportHeight)`
4. Shift painting position so bottom aligns with viewport bottom

**Important**: In `drawTextArea()`, when bottom-aligned with text wrap:
- Don't break loop early when `yPos > viewport()->height()`
- Continue drawing to ensure enough content for the shift

### 4. Text Wrap Toggle

When text wrap is toggled via `textWrapSet()`:
1. Reset `lastLineAligned_` state
2. Reset `actual_height_` to 0
3. Update scroll bars (range changes with wrap)
4. If was at bottom, restore bottom alignment
5. Force refresh

## Identified Bugs and Fixes

### Bug 1: `actual_height_` Not Initialized
**Problem**: When `paintEvent()` used `actual_height_` before `drawTextArea()` was called, it could be 0.
**Fix**: Added check `actual_height_ > 0` before using; fall back to `wholeHeight` otherwise.

### Bug 2: Race Condition in Resize
**Problem**: In `updateDisplaySize()`, bottom alignment was restored before pixmap was set up.
**Fix**: Moved bottom alignment restoration to after pixmap setup.

### Bug 3: Integer Underflow in Scroll Calculation
**Problem**: When `unwrappedLinesAtBottom > totalLines`, subtraction could underflow.
**Fix**: Added check `totalLines > unwrappedLinesAtBottom` before subtraction.

### Bug 4: Invalid Column Count During Init
**Problem**: `getNbBottomWrappedVisibleLines()` could have `visibleColumns <= 0` during init.
**Fix**: Added check and early return with `visibleLines` if columns invalid.

### Bug 5: Bottom Alignment State Not Reset on Toggle
**Problem**: `lastLineAligned_` wasn't reset when toggling text wrap.
**Fix**: Reset both `lastLineAligned_` and `actual_height_` in `textWrapSet()`.

### Bug 6: Content Shorter Than Viewport
**Problem**: When `effectiveHeight < viewport()->height()`, offset was negative-wrong-direction.
**Fix**: Set `drawingTopOffset_ = 0` when content fits in viewport.

### Bug 7: Long Wrapped Line Invisible After Resize (lastTopLine = 0 case)
**Problem**: When FilteredView has few lines (e.g., 1 line) that wrap to multiple visual lines (e.g., 3), and viewport is resized to show fewer visual lines (e.g., 2), the bottom portion of the wrapped line becomes invisible.
**Root Cause**: In `scrollContentsBy()`, the condition was:
```cpp
if ( ( lastTopLine.get() > 0 ) && scrollPosition.get() >= lastTopLine.get() )
```
When there's only 1 line, `lastTopLine = totalLines(1) - bottomWrappedLines(1) = 0`. The condition `lastTopLine.get() > 0` evaluates to `0 > 0` = FALSE, so we **never enter bottom alignment mode** even though we should.

**Fix**: Removed the `lastTopLine.get() > 0` check:
```cpp
if ( scrollPosition.get() >= lastTopLine.get() )
```
Now when `lastTopLine = 0` and `scrollPosition = 0`, we correctly enter bottom alignment mode, allowing the wrapped line to be shifted up to show its bottom portion.

**Affected Scenario**:
- FilteredView with very few matched lines
- Last line wraps to more visual lines than viewport can display
- Resize makes viewport shorter
- Previously: Bottom of wrapped line was cut off
- Now: Bottom of wrapped line is properly aligned to viewport bottom

### Bug 8: Window Unresponsive with Wrap Text (Performance Issue)
**Problem**: When text wrap is enabled, the window becomes unresponsive during scrolling.
**Root Cause**: The recent changes added a call to `getNbBottomWrappedVisibleLines()` inside `scrollContentsBy()`, which is called on EVERY scroll event. This function:
1. Iterates through lines from the bottom
2. Calls `getExpandedLineString()` for each line (expensive I/O operation)
3. Creates `WrappedString` objects for each line

This O(n) operation on every scroll caused severe performance degradation.

**Fix**: Removed `getNbBottomWrappedVisibleLines()` call from `scrollContentsBy()`. Instead, use the scroll bar's maximum value to determine if we're at the bottom:
```cpp
const bool atBottom = ( scrollMax > 0 ) ? ( scrollValue >= scrollMax ) : true;
```
The scroll range is already calculated correctly by `updateScrollBars()`, so we reuse that information instead of recalculating.

**Impact**: This was a regression introduced by recent changes. The original code did not have this performance issue.

### Bug 9: FilteredView Click Doesn't Center Line in Main View
**Problem**: When clicking a line in FilteredView with text wrap enabled, the corresponding line in the main view was not properly centered and could be off-screen.
**Root Cause**: Two functions had issues with text wrap:

1. `displayLine()` checked if a line was "on screen" using `getNbVisibleLines()`:
   ```cpp
   if ( ( line >= firstLine_ ) && ( line < ( firstLine_ + getNbVisibleLines() ) ) )
   ```
   This doesn't account for text wrap - a line might appear "on screen" by line number but actually be pushed below the viewport by wrapped content above it.

2. `jumpToLine()` calculated the center position using `getNbVisibleLines() / 2`, which doesn't account for wrapped lines taking more vertical space.

**Fix**: 
- In `displayLine()`: Always call `jumpToLine()` when text wrap is enabled
- In `jumpToLine()`: Use a smaller divisor (4 instead of 2) when text wrap is enabled to place the line in the upper portion of the viewport

### Bug 10: Bottom Alignment State Lost After Resize (scrollContentsBy not triggered)
**Problem**: After resizing, the last line could still be clipped even when bottom alignment should be active.
**Root Cause**: In `updateDisplaySize()`, when restoring bottom alignment:
```cpp
verticalScrollBar()->setValue( verticalScrollBar()->maximum() );
```
If the scroll value doesn't change (e.g., when scrollMax = 0 before and after resize), Qt's `setValue()` doesn't emit any signal, so `scrollContentsBy()` is never called. This meant `lastLineAligned_` and `firstLine_` weren't updated for the new viewport size.

**Fix**: Explicitly set the bottom alignment state in `updateDisplaySize()` when restoring bottom alignment, instead of relying on `scrollContentsBy()` being triggered:
```cpp
if ( wasBottomAligned && !followMode_ ) {
    lastLineAligned_ = true;
    firstLine_ = verticalScrollToLineNumber( newMax );
    verticalScrollBar()->setValue( newMax );
}
```

## Regression Analysis

The following bugs were **introduced by recent changes** (since commit 25c7de6d):
- **Bug 8 (Performance)**: Added `getNbBottomWrappedVisibleLines()` call in `scrollContentsBy()` - MAJOR REGRESSION
- **Bug 10 (State Lost)**: The resize handling logic was modified but didn't handle the edge case where `scrollContentsBy()` isn't triggered

The following bugs were **pre-existing** or **edge cases in the new logic**:
- **Bug 7 (lastTopLine = 0)**: Edge case in the new bottom alignment detection logic
- **Bug 9 (Click doesn't center)**: Pre-existing issue, but more noticeable with text wrap enabled

## Test Coverage

### Unit Tests Added (`tests/ui/crawlerwidget_test.cpp`)
- Text wrap enable/disable toggle test
- Search with text wrap enabled
- Click on filtered view line with text wrap (Bug 9 verification)
- Resize views with text wrap (Bug 8, 10 verification)

### Integration Test Fix (`tests/ui/logfiltereddata_test.cpp`)
- Fixed intermittent heap corruption (`0xc0000374`) on Windows
- Root cause: Signal throttler timer firing after LogFilteredData destruction
- Fix: Added `QTest::qWait(150)` after search completes to allow pending signals to process

## Debug Logging

All text wrap related logging uses tags for easy filtering:

| Tag | Location | Purpose |
|-----|----------|---------|
| `[TextWrap:Calc]` | `getNbBottomWrappedVisibleLines()` | Wrapped line calculations |
| `[TextWrap:ScrollBar]` | `updateScrollBars()` | Scroll range updates |
| `[TextWrap:Scroll]` | `scrollContentsBy()` | Scroll position and alignment |
| `[TextWrap:Paint]` | `paintEvent()` | Rendering with offsets |
| `[TextWrap:Resize]` | `updateDisplaySize()` | Resize handling |
| `[TextWrap:Draw]` | `drawTextArea()` | Content drawing |
| `[TextWrap:Toggle]` | `textWrapSet()` | Wrap mode changes |

To enable debug logging, set log level to Debug in Klogg settings.

## Verification Plan

### Test Case 1: Basic Text Wrap
1. Open a file with long lines
2. Enable "Wrap text" from View menu
3. Verify lines wrap at viewport boundary
4. Verify line numbers stay consistent

### Test Case 2: Bottom Alignment
1. Open file with few lines (fits in viewport)
2. Enable "Wrap text"
3. Scroll to bottom
4. Verify last line is at bottom of viewport
5. Resize window (smaller, larger)
6. Verify last line stays visible and at bottom

### Test Case 3: Resize While Bottom-Aligned
1. Scroll to bottom of a large file
2. Enable "Wrap text"
3. Make window shorter → verify last line still visible
4. Make window taller → verify last line stays at bottom with more context visible
5. Verify scrolling up works (text moves)

### Test Case 4: Toggle While Bottom-Aligned
1. Scroll to bottom
2. Enable "Wrap text" → verify stays at bottom
3. Disable "Wrap text" → verify stays at bottom
4. Enable again → verify stays at bottom

### Test Case 5: Filtered View
1. Apply a search filter
2. Enable "Wrap text" in filtered view
3. Verify filtered results wrap correctly
4. Scroll to bottom, resize → verify behavior matches main view

### Test Case 6: Very Long Lines
1. Open file with lines longer than 3x viewport width
2. Enable "Wrap text"
3. Scroll to bottom where last line wraps to many visual lines
4. Verify complete last line is visible

### Test Case 7: FilteredView with Single Long Wrapped Line (Bug 7 Regression Test)
1. Open a log file and apply a filter that matches only 1 line
2. Ensure the matched line is very long (wraps to 3+ visual lines)
3. Enable "Wrap text"
4. Resize FilteredView to be very short (e.g., 2 visual lines height)
5. **Verify**: The BOTTOM portion of the wrapped line is visible (not the top)
6. The last visual line of the wrapped content should be at the bottom of the viewport
7. Resize FilteredView taller → verify more of the wrapped line becomes visible
8. Resize FilteredView shorter again → verify bottom portion stays visible

## Architecture Diagram

```
┌─────────────────────────────────────────────────────────────────┐
│                        MainWindow                               │
│  ┌───────────────────────────────────────────────────────────┐  │
│  │                    CrawlerWidget                          │  │
│  │  ┌─────────────────────────────────────────────────────┐  │  │
│  │  │              LogMainView                            │  │  │
│  │  │         (inherits AbstractLogView)                  │  │  │
│  │  │  - Overview widget                                  │  │  │
│  │  │  - Full file display                                │  │  │
│  │  └─────────────────────────────────────────────────────┘  │  │
│  │                                                           │  │
│  │  ┌─────────────────────────────────────────────────────┐  │  │
│  │  │              FilteredView                           │  │  │
│  │  │         (inherits AbstractLogView)                  │  │  │
│  │  │  - Filtered results display                         │  │  │
│  │  │  - Tabs for multiple filters                        │  │  │
│  │  └─────────────────────────────────────────────────────┘  │  │
│  └───────────────────────────────────────────────────────────┘  │
└─────────────────────────────────────────────────────────────────┘

                    AbstractLogView
                          │
          ┌───────────────┴───────────────┐
          │                               │
     LogMainView                    FilteredView
     
Text Wrap Data Flow:
┌──────────────┐     ┌──────────────────┐     ┌─────────────────┐
│ User Action  │────▶│ updateScrollBars │────▶│getNbBottomWrapped│
│(resize/scroll)     │                  │     │  VisibleLines   │
└──────────────┘     └──────────────────┘     └─────────────────┘
                              │
                              ▼
┌──────────────┐     ┌──────────────────┐     ┌─────────────────┐
│  paintEvent  │◀────│scrollContentsBy  │◀────│verticalScrollBar│
│              │     │  (set alignment) │     │   setValue()    │
└──────────────┘     └──────────────────┘     └─────────────────┘
        │
        ▼
┌──────────────┐     ┌──────────────────┐
│ drawTextArea │────▶│ WrappedString    │
│(render lines)│     │(wrap calculation)│
└──────────────┘     └──────────────────┘
```

## Related Files

- `src/ui/include/abstractlogview.h` - Main view class header
- `src/ui/src/abstractlogview.cpp` - Main view implementation
- `src/ui/include/wrappedstring.h` - Text wrapping logic
- `src/ui/src/filteredview.cpp` - Filtered view implementation  
- `src/ui/src/logmainview.cpp` - Main view implementation
- `src/ui/src/crawlerwidget.cpp` - Widget containing both views
- `src/settings/include/configuration.h` - Text wrap setting storage
