User Interface
==============

Shortcut notation: :kbd:`Ctrl` means :kbd:`Ctrl` on Windows/Linux and
:kbd:`Cmd` on macOS. Platform-specific shortcuts spell out :kbd:`Cmd` when
they differ from this convention.

Widgets
-------

Apart from the main display, SpeedCrunch offers a number of extra panels, referred to as *widgets* here.
Most of them are dockable panels that can be moved around the main window and enabled or disabled via the
:menuselection:`View` menu.

* Formula Book
    The formula book provides access to commonly used formulas and calculations. Simply insert
    a formula into the expression editor by clicking on it.

    You can help expanding the formula book by posting your requests to the `issue tracker <tracker_>`_.

* Constants
    The constants widget shows a large list of scientific constants (including CODATA-based entries),
    grouped by category and subcategory.

    The list columns are:

    * ``Name``
    * ``Symbol`` (when available)
    * ``Value``
    * ``Unit`` (when available)
    * ``Identifier`` (built-in constant key, typically ``k_*``)

    Use the ``Category`` and ``Subcategory`` selectors to narrow the list.
    The search field matches by name, symbol, and identifier.

    Double-clicking an entry inserts its ``Identifier`` into the editor.

* User Variables
    The variables widget lists all :ref:`user-defined variables <variables>`. Any of them can be inserted into the editor by double-clicking it.
    Additionally, it is possible to delete a variable by selecting it and pressing the :kbd:`Delete` key on your keyboard.

* Functions and User Functions
    Similar to the variables widget, these show built-in and :ref:`user-defined functions <user_functions>` respectively.

* User Units
    Similar to the user functions widget, this lists :ref:`user-defined units <user_units>`.
    Double-click inserts ``[name]`` in the editor, and the context menu supports edit/delete operations.

* Bit Field
    The bit field dock is designed to make working with binary numbers easier. It shows a field of 64 squares,
    each representing a bit in the current result. Any bit can be toggled by clicking its square; the resulting
    number is automatically inserted into the editor. Additional buttons allow you to shift, invert and reset all the bits at once.
    While the mouse cursor is hovering over the bit field, scrolling the mouse wheel will also shift the bits.

  .. _keypad:
* Keypad
    The on-screen keypad provides clickable input for common operations and symbols. It supports multiple presets
    and a custom layout, so you can adapt it to your workflow. You can also adjust the keypad zoom level for better
    readability or touch area on touchscreens. For fast and full-featured input, SpeedCrunch's keyboard interface is still recommended.

    Available entries in :menuselection:`View --> Keypad` are:

    * ``Basic`` (**default**)
    * ``Scientific (wide)``
    * ``Scientific (narrow)``
    * ``Custom...``
    * ``Disable`` (shown as ``Disabled`` when selected)

    Available entries in :menuselection:`View --> Keypad --> Zoom` are:

    * ``100%`` (**default**)
    * ``150%``
    * ``200%``

    The zoom level scales the keypad buttons, including their text.

    ``Custom...`` opens a dialog where you can define the keypad matrix size (rows and columns)
    and configure each button individually. For each position in the matrix, you can set:

    * The button label shown in the keypad
    * The button behavior: ``Insert text``, ``Backspace``, ``Clear expression``, or ``Evaluate expression``
    * The text to insert (for ``Insert text`` behavior)

    The dialog also includes a ``Copy preset`` selector with an :guilabel:`Apply` button,
    so you can start from the ``Basic``, ``Scientific (wide)``, or
    ``Scientific (narrow)`` layout and then customize it.

    Custom keypad settings are saved and restored automatically.

    You can also right-click directly on the keypad to open the same menu as
    :menuselection:`View --> Keypad`.

    .. versionadded:: 1.0

    .. versionchanged:: 0.11
       The keypad was removed in SpeedCrunch 0.11; however, it was added back in 0.12.

* History
    The history widget lists all previous inputs. Double-click a line to recall it. Note that the main result display also provides this functionality.

* Main Menu
    Toggle the main menu bar visibility via :menuselection:`View --> Main Menu`.
    This option is available on Windows and Linux.
    On macOS, this option is not available because the application menu is managed by the system menu bar (outside the window), so SpeedCrunch cannot hide it like on Windows and Linux.
    On Windows and Linux, when the top main menu is hidden, a navigable Main Menu
    (mirroring the top menu structure) is available from the right-click context
    menu on the result-display.

    .. versionadded:: 1.0

.. _tracker: https://bitbucket.org/heldercorreia/speedcrunch/issues


Session Tabs and Panes
----------------------

When a pane contains more than one session, SpeedCrunch shows a tab bar above
the result display. Click a tab to switch sessions, drag tabs horizontally to
reorder them within the pane, or drag a tab away from the tab bar to move it to
another pane. Dropping a dragged tab outside the current window creates a new
window for that session; dropping it on a pane in another SpeedCrunch window
moves it there.
Use :menuselection:`Session --> New Window` to open a blank
session in a new window that copies the current window and dock layout.


Expression Editor Features
--------------------------

The expression editor provides some advanced features:

* Autocompletion
    If you start typing a name (e.g. of a variable, function, or unit), a pop-up with matching names will appear. Press :kbd:`Tab` to insert the current
    suggestion. Pressing :kbd:`Enter` evaluates the expression by default. If you explicitly navigate the popup with arrow/page/home/end keys or click a
    suggestion with the mouse, then :kbd:`Enter` accepts that selected suggestion. Pressing :kbd:`Escape` dismisses the autocomplete popup.

* Function signature tooltip
    While typing a function call, the live result area can show the function signature (parameter list). The parameter currently being edited is highlighted.
    This also works for user-defined functions. Pressing :kbd:`Escape` will dismiss the tooltip.

* Quick constant insertion
    Press :kbd:`Ctrl+Space` to open a list of constants that allows quick access to the same constants as the constants widget (see above).
    Use the arrow keys to navigate the list. Pressing :kbd:`Escape` will dismiss this popup.
    Selecting an entry inserts its identifier (for example ``k_speed_of_light_in_vacuum``).

  .. _context-help:
* Context help
    Pressing :kbd:`F1` will show the manual page for the function under the cursor, providing quick access to detailed
    usage information for a function. Pressing :kbd:`Escape` will dismiss the manual window again.

* Selection results
    If :menuselection:`Settings --> Results --> Show Live Result Preview` is enabled, selecting a partial expression in the expression editor or in the result display will show you the result of the selected expression.
    You can dismiss this preview by pressing :kbd:`Escape` or by clicking the :guilabel:`×` button at the top-right corner of the preview.


Import/Export
-------------

SpeedCrunch automatically stores your sessions and lets you restore them with :menuselection:`Session --> Open`.
Use :menuselection:`Session --> Open Sessions Folder` to open the folder where SpeedCrunch stores these session files
when you want to back them up or clean them up manually.
You can also export the current session as JSON (:menuselection:`Session --> Export --> JSON`) and later import it
with :menuselection:`Session --> Import`. The data is stored in a SpeedCrunch-specific file format. [#f1]_ While the
session files are human-readable, they are designed for use by SpeedCrunch. If you want to export your calculations
to work on them in another program or hand them to a colleague, the other export options are preferable.

Import validates the selected JSON file as a SpeedCrunch session and opens it as a new tab. If the file is not valid
SpeedCrunch session JSON, SpeedCrunch shows an error and leaves the current session unchanged.

You can export the session as HTML (:menuselection:`Session --> Export --> HTML`). The resulting file will consist of the contents of the result
display and can be viewed in any web browser. This feature can also be used to print a SpeedCrunch session by printing the exported
HTML document. Since the syntax highlighting and color scheme are maintained in the HTML output, it is recommended to select a color scheme
with a white background (e.g. *Standard*) prior to exporting if you intend to print the document.

The final, most basic option is to export your session as a plain text file (:menuselection:`Session --> Export --> Plain text`).
In contrast to the HTML export option, the syntax highlighting will be lost.

User Definitions
++++++++++++++++++++++++

.. versionadded:: 1.0

To define user variables, user functions and user units that are loaded globally, use
:menuselection:`Settings --> Symbols --> User Definitions...`.

This dialog provides:

* A multi-line editor (one definition per line), with syntax highlighting and line numbers.
* A clear reminder that these definitions are global, loaded into every session and override same-name session definitions.
* :guilabel:`Apply` to apply definitions immediately.
* :guilabel:`Validate` to validate and preview results without applying.

When applying or testing, SpeedCrunch reports:

* Imported variable count
* Imported function count
* Imported unit count
* Line numbers with errors

Invalid definitions are ignored. Variable definitions that evaluate to ``NaN`` are also ignored.


Settings
--------

SpeedCrunch's behavior can be customized to a large degree using the configuration options in the
:menuselection:`Settings` menu. This section explains the settings that are available.

Result Display Interactions
+++++++++++++++++++++++++++

The result display supports mouse-driven interactions for navigation and editing.

* Right-click context menu
    Right-clicking an expression/calculation block in the result display opens a context menu
    for that specific block.
    This menu includes an entry to toggle the main menu bar visibility.

    .. versionadded:: 1.0

* Hover highlighting of calculation blocks
    When :menuselection:`Settings --> Appearance --> Hover Highlighting` is enabled, moving the mouse over
    the result display highlights the calculation block under the cursor.
    The highlighted block also shows quick-action icons for deleting, editing, and copying.

    .. versionadded:: 1.0

* History rewriting from highlighted blocks
    From the hovered/highlighted calculation block, you can re-edit an earlier expression and apply it
    as a history rewrite. SpeedCrunch then recalculates all expressions below that edited entry.

    .. versionadded:: 1.0

* Calculation settings from highlighted blocks
    The calculation settings action opens a tabular layout with one mandatory
    ``Main Line`` row and four optional extra result-line rows. The table
    shows line enablement, notation, and decimal places together so the stored
    calculation context can be adjusted before SpeedCrunch
    recalculates the affected history.

.. _result_format:

Notation
+++++++++++++

This section allows selecting the result notation to use (for example in
:menuselection:`Settings --> Results --> Notation & Precision...` or from the
status-bar notation selector). You can select one of the following options.
On first launch, the default is :menuselection:`Automatic decimal`.

* :menuselection:`Automatic decimal`
    Use fixed-point decimal form for most results; values with exponent larger than 14 or smaller than -9 are shown in
    scientific notation. With a finite precision setting, very small values may switch to scientific notation earlier.
* :menuselection:`Fixed-point decimal`
    Display results in fixed-point decimal form. For excessively
    large or small numbers, this format may still fall back to scientific notation.
* :menuselection:`Engineering decimal`
    Display results in engineering notation. This is a variant of :ref:`scientific notation <scientific_notation>` in which
    the exponent is divisible by three.
* :menuselection:`Scientific decimal`
    Display results in :ref:`normalized scientific notation <scientific_notation>`.
* :menuselection:`Rational`
    Try to display real results as fractions (for example, ``1/3``) using continued-fraction approximation with bounded denominator. If no close match is found, results are displayed in decimal form.
    For common trigonometric and inverse-trigonometric results, SpeedCrunch prefers
    symbolic exact forms such as ``pi/2``, ``7⋅pi/6``, ``1/2``, ``sqrt(2)/2``,
    and ``sqrt(3)/3``.
    Example::

        arcsin(1)
        = pi/2

        sin(45)    (angle mode = degree)
        = √(2)/2

        arcsin(sqrt(3)/22)
        = 0.07881114207211010205

    .. versionadded:: 1.0
* :menuselection:`Binary`
    Display results as binary numbers, i.e. in base-2.
* :menuselection:`Octal`
    Display all results as octal numbers, i.e. in base-8.
* :menuselection:`Hexadecimal`
    Display all results as hexadecimal numbers, i.e. in base-16.
* :menuselection:`Sexagesimal`
    Display dimensionless and time results as :ref:`sexagesimal values <sexagesimal_values>`, i.e. with minutes and seconds. All other results are displayed in fixed-point decimal form.

    .. versionadded:: 1.0

Angle Mode
++++++++++

Select the angular unit to be used in calculations. For functions that operate on angles, notably the
:ref:`trigonometric functions <trigonometric>` like :func:`sin` or :func:`cos`, this setting
determines the angle format of the arguments.

* :menuselection:`Radian`
    Use radians for angles. A full circle corresponds to an angle of 2π radians.
* :menuselection:`Degree`
    Use degrees for angles. A full circle corresponds to an angle of 360°.
* :menuselection:`Gradian`
    Use gradians for angles. A full circle corresponds to an angle of 400 gradians.

    .. versionadded:: 1.0

* :menuselection:`Turn`
    Use turns for angles. A full circle corresponds to an angle of 1 turn.
* :menuselection:`Revolution`
    Use revolutions for angles. A full circle corresponds to an angle of
    1 revolution.

    .. versionadded:: 1.0
    
.. _complex_numbers:

Complex Numbers
+++++++++++++++

Configure global complex-number behavior via
:menuselection:`Settings --> Results --> Complex Numbers`.
Complex numbers are always enabled.

* :menuselection:`Imaginary Unit --> i`
    Use ``i`` as the imaginary unit symbol.
* :menuselection:`Imaginary Unit --> j`
    Use ``j`` as the imaginary unit symbol.
* :menuselection:`Form --> Rectangular (a + bi)`
    Display complex results in rectangular form.
* :menuselection:`Form --> Exponential (reⁱᶿ)`
    Display complex results in exponential form.
* :menuselection:`Form --> Trigonometric (r(cos θ + i·sin θ))`
    Display complex results in trigonometric form.
* :menuselection:`Form --> Cis (r·cis(θ))`
    Display complex results in cis form.
* :menuselection:`Form --> Phasor (r∠θ)`
    Display complex results in phasor form.

When the angle mode is set to radians, simple phase multiples of ``pi`` are
shown symbolically in these complex forms. Other angle modes show their numeric
phase values.

.. _radix_character:

Number Format
+++++++++++++

Choose how decimal numbers are displayed and interpreted via
:menuselection:`Settings --> Results --> Number Format...`.

.. versionadded:: 1.0

This dialog replaces the older separate Radix Character and Digit Grouping
menus with a single combined Number Format selection.

The default option is no digit grouping with dot decimal separator
(``1234567.12345``).

Additional options let you choose:

* no digit grouping with dot or comma decimal separator
* SI grouping with space separators (integer and fractional grouped in 3s)
* 3-digit grouping with comma, dot, space, or underscore
* Indian/South Asian 3-2-2 grouping with comma separators

Some options include grouped fractional digits; those are separate explicit
choices.

The apostrophe (``'``), commonly used as a thousands separator in some
locales and contexts (for example Switzerland, Liechtenstein, and some
technical writing in Austria/Germany), is intentionally not offered because
SpeedCrunch reserves it for :ref:`sexagesimal notation <sexagesimal_values>`.

For input, SpeedCrunch remains permissive across styles to support
copy/paste from other applications. In clear mixed-separator cases, it
accepts the number and normalizes it by ignoring non-digit grouping
characters as needed.

History Size Limit
++++++++++++++++++

This setting controls how calculation history is stored for the active session.
It is available from :menuselection:`Session --> History Size Limit...`.

Calculation history is saved automatically when a session changes and restored
on the next launch.

The history size limit sets the maximum number of stored history entries for
that session. By default, SpeedCrunch keeps up to 1000 entries per session and
automatically removes the oldest ones when this limit is exceeded. When a
calculation fills the last available history slot, SpeedCrunch warns that future
calculations will remove the oldest calculation from that session. Set the value
to ``0`` to disable the limit for the active session.

.. versionadded:: 1.0


Window
++++++

This section contains settings that control the main window behavior.

* :menuselection:`Save Window Position on Exit`
    Controls if the window position is saved and restored.
* :menuselection:`Always on Top`
    Keep the SpeedCrunch window on top of other windows. This option is hidden on
    Wayland (non-X11 Linux desktop sessions), because many Wayland compositors do
    not honor it reliably. It remains available on Windows, macOS, and Linux X11
    sessions.


Results
+++++++

This section contains settings that control result output and post-evaluation behavior.

* :menuselection:`Show Live Result Preview`
    If set, SpeedCrunch will display partial results as you type your expression as well
    as results when selecting a partial expression in the editor.

* :menuselection:`Number Format...`
    Open the number-format dialog and pick one combined setting for decimal
    separator and digit grouping. See :ref:`radix_character`.

* :menuselection:`Notation & Precision...`
    Open a tabular layout with one mandatory ``Main Line`` row and four
    optional extra result-line rows. Each row has columns for enabling the
    line, notation, and decimal places. Extra lines are ignored unless their
    row is enabled.

    Applying changes from this dialog updates only the current editor previews
    immediately (live/selection) without rewriting already displayed
    calculation history.

* :menuselection:`Rounding Mode`
    Choose how displayed results are rounded when formatting to a finite number
    of digits. The default is :menuselection:`Nearest, Half Away (round)`.
    Available modes are:
    :menuselection:`Nearest, Half Away (round)`,
    :menuselection:`Nearest, Half Even (roundeven)`,
    :menuselection:`Toward Zero (trunc)`,
    :menuselection:`Toward +∞ (ceil)`,
    :menuselection:`Toward −∞ (floor)`.

    * :menuselection:`Nearest, Half Away (round)`
        Round to nearest; ties increase magnitude. This matches the
        :func:`round` function.
        Example at 1 fractional digit: ``2.250 -> 2.3``,
        ``-2.250 -> -2.3``.
    * :menuselection:`Nearest, Half Even (roundeven)`
        Round to nearest; ties go to the nearest even last kept digit. This is
        Banker's rounding and matches the :func:`roundeven` function.
        Example at 1 fractional digit: ``2.250 -> 2.2``,
        ``2.350 -> 2.4``, ``-2.250 -> -2.2``.
    * :menuselection:`Toward Zero (trunc)`
        Discard extra digits without rounding up. This matches the
        :func:`trunc` function.
        Example at 1 fractional digit: ``2.290 -> 2.2``,
        ``-2.290 -> -2.2``.
    * :menuselection:`Toward +∞ (ceil)`
        Always round upward. This matches the :func:`ceil` function.
        Example at 1 fractional digit: ``2.210 -> 2.3``,
        ``-2.210 -> -2.2``.
    * :menuselection:`Toward −∞ (floor)`
        Always round downward. This matches the :func:`floor` function.
        Example at 1 fractional digit: ``2.290 -> 2.2``,
        ``-2.210 -> -2.3``.

    This setting affects result formatting (including live previews), not the
    numeric value of expressions. Use the named function in parentheses when a
    calculation must apply the same rounding strategy explicitly. Like other
    result-format settings, changes apply to subsequent calculations and
    previews; already stored history entries keep their original display.

* :menuselection:`Complex Numbers`
    Configure the imaginary unit and complex result form. See
    :ref:`complex_numbers`.

* :menuselection:`Unit Notation`
    Configure whether unit results use exponential notation, such as
    ``m·s⁻¹``, or fractional notation, such as ``m/s``.
* :menuselection:`Automatically Copy New Results to Clipboard`
    Automatically copy each newly evaluated result to the clipboard.
* :menuselection:`Simplify Displayed Expressions`
    When enabled (default), SpeedCrunch adds an extra symbolic line before
    numeric results, simplifying the interpreted expression for readability
    (for example, combining repeated factors into powers and folding simple
    constant terms). This interpreted line also makes implicit-multiplication
    association explicit. Disable this option to keep only the unsimplified
    interpreted expression and numeric results.

    Entering the expression ``2.1 − 1 cos(pi)^2 cos(pi) 5 cos pi  / −cos^2(pi) + 3.4``
    gives the following interpretation and simplification:

    .. code-block:: text

      2.1 − 1 ⋅ cos²(π) ⋅ cos(π) ⋅ 5 ⋅ cos(π) / (−cos²(π)) + 3.4
      = 5 ⋅ cos²(π) + 5.5
      = 10.5

    .. versionadded:: 1.0


Editing
+++++++

* :menuselection:`Autocomplete`
    Controls which entities are included in editor autocompletion.
    All options are enabled by default.

    * ``Built-in functions`` includes built-in function names.
    * ``Built-in variables`` includes built-in constant names.
    * ``Units`` includes spelled-out unit names (for example, ``meter``).
    * ``User functions`` includes user-defined function names.
    * ``User variables`` includes user-defined variable names.

    The autocomplete popup also shows category icons for suggestions.

    .. versionadded:: 1.0

.. _automatic_result_reuse:

* :menuselection:`Auto-Insert "ans" When Starting with an Operator`
    If a new expression starts with ``+``, ``-``, ``*``, or ``/``, SpeedCrunch inserts ``ans`` first.
* :menuselection:`Show Empty History Hint`
    Show or hide the ``Type an expression here`` hint when there are no calculations in history.
* :menuselection:`Keep Entered Expression After Evaluate`
    If selected, the entered expression remains in the editor after evaluating it.
* :menuselection:`Up/Down Arrow History`
    Controls whether :kbd:`Up` and :kbd:`Down` in the editor navigate history:

    * ``Never`` always moves the cursor inside the editor.
    * ``Always`` (default) always navigates history.
    * ``Only for Single-Line Expressions`` navigates history only when the
      expression fits in one visual line; otherwise, it moves the cursor.

    .. versionadded:: 1.0

User Interface Settings
+++++++++++++++++++++++

* :menuselection:`Settings --> Appearance --> Theme...`
    Open the theme dialog. Themes are listed in :guilabel:`Light Themes` and
    :guilabel:`Dark Themes` groups. Selecting a theme updates the preview
    immediately.

    The :guilabel:`Preview` area shows representative result-display and
    editor content so that syntax-highlighting, result, and background colors
    are visible while editing. The selected ``background`` color generates
    nearby OKLCH surface colors for the expression editor and surrounding
    application chrome, followed by progressively offset surface colors for
    dock titles, dock controls and table headers, dock search and content
    areas, pane splitters, and scrollbars. It also generates the
    primary/accent color unless the theme specifies a ``primary`` color.

    The :guilabel:`Colors` area provides per-role color selection buttons showing
    both the ``#RRGGBB`` value and the actual color. Changing any color creates a
    custom theme based on the current selection.

    The dialog also includes:

    * :guilabel:`Import...` to copy a theme JSON file into SpeedCrunch's
      user theme directory and refresh the theme lists;
    * :guilabel:`Export...` to save the current theme as JSON in the same user
      theme directory.

    Import and export refuse theme names that collide with built-in themes.
    Import asks for confirmation before overwriting an existing custom theme.

    SpeedCrunch also supports loading additional theme files from the following
    directory:

    * Windows: :file:`C:/Users/<USERNAME>/AppData/Roaming/SpeedCrunch/color-schemes/`
    * Linux/Unix: :file:`~/.local/share/SpeedCrunch/color-schemes/`
    * Linux (Flatpak): :file:`~/.var/app/org.speedcrunch.SpeedCrunch/data/SpeedCrunch/color-schemes/`
    * OS X: :file:`~/Library/Application Support/SpeedCrunch/color-schemes/`
    * Portable version (any OS): :file:`color-schemes` subdirectory in the
      application directory.

    Theme files use JSON and map role names to color values. Supported role names are:
    ``number``, ``parens``, ``list``, ``unit``, ``result``, ``comment``,
    ``function``, ``operator``, ``variable``, ``separator``, ``background``,
    and optional ``primary``. The editor surface and scrollbar colors are
    generated from ``background``.
    For the full schema, see :doc:`theme_json_schema`.

    .. versionadded:: 1.0
       The :menuselection:`Theme...` dialog, including live preview and JSON import/export.
* :menuselection:`Settings --> Appearance --> Font`
    Select the font to use for the expression editor and result display.
* :menuselection:`Settings --> Appearance --> Syntax Highlighting`
    Enable or disable syntax highlighting.
* :menuselection:`Settings --> Appearance --> Hover Highlighting`
    Enable or disable hover highlighting.
* :menuselection:`Settings --> Language`
    Select the user interface language.

* :menuselection:`Settings --> Check for Updates`
    Trigger an update check and show information when a newer version is available.

    .. versionadded:: 1.0


Keyboard Shortcuts
------------------

Editing
+++++++
* :kbd:`Ctrl+O`
    Open session.
* :kbd:`Ctrl+Q`
    Quit SpeedCrunch.
* :kbd:`Ctrl+N`
    Create a new session in the current session pane.
* :kbd:`Ctrl+T` (:kbd:`Cmd+T` on macOS)
    Create a new session tab in the current session pane.
* :kbd:`Ctrl+Shift+T`
    Reopen the most recently closed session tab.
* :kbd:`Ctrl+C`
    Copy selected text to clipboard.
* :kbd:`Ctrl+R`
    Copy last result to clipboard.
* :kbd:`Ctrl+V`
    Paste from clipboard.
* :kbd:`Ctrl+A`
    Select entire expression.
* :kbd:`Ctrl+(` or :kbd:`Ctrl+)`
    Wrap the current selection in parentheses. If no text is selected, the entire expression is wrapped.

Session Tabs
++++++++++++

* :kbd:`Cmd+Alt+Left` and :kbd:`Cmd+Alt+Right` (macOS)
    Move to the previous or next session tab or pane.
* :kbd:`Ctrl+Page Up` and :kbd:`Ctrl+Page Down` (Windows/Linux)
    Move to the previous or next session tab or pane.

Widgets and Docks
+++++++++++++++++

* :kbd:`Ctrl+1`
    Show/hide formula book.
* :kbd:`Ctrl+2`
    Show/hide constants widget.
* :kbd:`Ctrl+3`
    Show/hide functions widgets.
* :kbd:`Ctrl+4`
    Show/hide variables widget.
* :kbd:`Ctrl+5`
    Show/hide user functions widget.
* :kbd:`Ctrl+6`
    Show/hide user units widget.
* :kbd:`Ctrl+7`
    Show/hide history widget.
* :kbd:`Ctrl+8`
    Show/hide bit field dock.
* :kbd:`Ctrl+B`
    Show/hide the status bar.
    The status bar provides quick selectors for :menuselection:`Angle Mode`,
    :menuselection:`Results --> Notation`, and
    :menuselection:`Results --> Precision`. On narrow windows, selectors that
    do not fully fit are hidden automatically.

Scrolling
+++++++++

* :kbd:`Page Up` and :kbd:`Page Down`
    Scroll the result window page-wise.
* :kbd:`Shift+Page Up` and :kbd:`Shift+Page Down`
    Scroll the result window line-wise.
* :kbd:`Cmd+Page Up` and :kbd:`Cmd+Page Down` (macOS)
    Scroll to the top or bottom of the result window.

Focus Navigation
++++++++++++++++

* :kbd:`F6` and :kbd:`Shift+F6`
    Move focus forward or backward between the expression editor and visible dock controls.


Notation
++++++++

* :kbd:`F2`
    Set result notation to automatic decimal.
* :kbd:`F3`
    Set result notation to fixed-point decimal.
* :kbd:`F4`
    Set result notation to engineering decimal.
* :kbd:`F5`
    Set result notation to scientific decimal.
* :kbd:`F7`
    Set result notation to octal.
* :kbd:`F8`
    Set result notation to hexadecimal.
* :kbd:`F9`
    Set result notation to sexagesimal.
* :kbd:`F10`
    Set result notation to binary.

    .. versionadded:: 1.0
    
Various
+++++++

* :kbd:`F1`
    Show context help (dismiss with :kbd:`Escape`).

    .. versionadded:: 0.12

* :kbd:`F11`
    Toggle full screen.
* :kbd:`Ctrl` + mouse wheel, :kbd:`Shift` + mouse wheel, or :kbd:`Shift+Up` and :kbd:`Shift+Down`
    Change the font size.
* :kbd:`Ctrl+Shift` + mouse wheel
    Change the window opacity.

    .. versionadded:: 0.12


.. rubric:: Footnotes

.. [#f1] Starting with SpeedCrunch 0.12, the session format is based on `JSON <json_>`_. Previous
         versions used a simple custom text format.

.. _json: http://json.org/
