User Interface
==============

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
    On Windows and Linux, a navigable Main Menu (mirroring the top menu structure) is also available
    from the right-click context menu on the result-display. This is useful when you prefer to keep
    the top main menu hidden but still want quick access to menu actions.

    .. versionadded:: 1.0

.. _tracker: https://bitbucket.org/heldercorreia/speedcrunch/issues


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
    Press :kbd:`Control+Space` to open a list of constants that allows quick access to the same constants as the constants widget (see above).
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

SpeedCrunch can save/export your session in a number of ways. The :menuselection:`Session --> Save` and :menuselection:`Session --> Load` menu entries
allow you to easily save and restore your entire SpeedCrunch session. The data is stored in a SpeedCrunch-specific file format. [#f1]_
While the session files are human-readable, they are designed for use by SpeedCrunch. If you want to export your
calculations to work on them in another program or hand them to a colleague, the other export options are preferable.

You can save the session as HTML (:menuselection:`Session --> Export --> HTML`). The resulting file will consist of the contents of the result
display and can be viewed in any web browser. This feature can also be used to print a SpeedCrunch session by printing the exported
HTML document. Since the syntax highlighting and color scheme are maintained in the HTML output, it is recommended to select a color scheme
with a white background (e.g. *Standard*) prior to exporting if you intend to print the document.

The final, most basic option is to export your session as a plain text file (:menuselection:`Session --> Export --> Plain text`).
In contrast to the HTML export option, the syntax highlighting will be lost.

SpeedCrunch also offers capabilities to *import* a session from a text file (:menuselection:`Session --> Import`).
Select any plain text file and SpeedCrunch will try to evaluate each line of the file as if the user entered it directly.

Startup Definitions
++++++++++++++++++++++++

.. versionadded:: 1.0

To define user variables, user functions and user units that are loaded automatically at startup, use
:menuselection:`Session --> Startup Definitions...`.

This dialog provides:

* A multi-line editor (one definition per line), with syntax highlighting and line numbers.
* Merge/overwrite behavior selection for name collisions.
* A warning banner when overwrite mode is selected.
* :guilabel:`Apply Now` to apply definitions immediately.
* :guilabel:`Test Now` to validate and preview results without applying.
* :guilabel:`Import...` / :guilabel:`Export...` to share startup definitions as plain text (``.txt``).
* An advanced option to apply definitions before session restore (instead of after restore).

When applying or testing, SpeedCrunch reports:

* Imported variable count
* Imported function count
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
    
Complex Numbers
+++++++++++++++

Configure global complex-number behavior via
:menuselection:`Settings --> Complex Numbers`.
On first launch, complex numbers are disabled by default.

* :menuselection:`Disable` (shown as ``Disabled`` when selected)
    Disable complex-number output globally.
* :menuselection:`Imaginary Unit i`
    Use ``i`` as the imaginary unit symbol.
* :menuselection:`Imaginary Unit j`
    Use ``j`` as the imaginary unit symbol.

Complex output form (Rectangular/Polar) is selected per result line in
:menuselection:`Settings --> Results --> Notation & Precision...`.

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

History
+++++++

This section contains settings that control how calculation history is stored.

* :menuselection:`History Saving --> Never`
    Do not persist the calculation history between runs.
* :menuselection:`History Saving --> On Exit`
    Save the calculation history when SpeedCrunch exits and restore it on the next launch.
* :menuselection:`History Saving --> Continuously`
    Save calculation history after each new calculation and restore it on the next launch.
* :menuselection:`History Saving`
    This preference only affects calculation history. User-defined functions and user-defined
    variables are always persisted.

    .. versionadded:: 1.0
* :menuselection:`History Size Limit...`
    Sets the maximum number of stored history entries. By default, SpeedCrunch keeps
    up to 100 entries and automatically removes the oldest ones when this limit is
    exceeded. Set the value to ``0`` to disable the limit.

    .. versionadded:: 1.0


Window
++++++

This section contains settings that control the main window behavior.

* :menuselection:`Save Window Position on Exit`
    Controls if the window position is saved and restored.
* :menuselection:`Single Instance`
    When enabled (default), launching SpeedCrunch while it is already running focuses
    the existing window instead of starting a second instance.
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
    Open a unified dialog to configure notation, precision, and complex form.
    The precision row label changes by notation: ``Decimal places`` for
    decimal notations, otherwise ``Fractional digits``.
    By default, the dialog shows only ``Main Line`` settings. Enable
    ``Advanced mode: show multiple result lines`` to configure
    ``Extra Line #1`` through ``Extra Line #4``.

    When advanced mode is disabled, configured extra lines are preserved but
    ignored in output.

    Applying changes from this dialog updates only the current editor previews
    immediately (live/selection) without rewriting already displayed
    calculation history.
* :menuselection:`Rounding Mode`
    Choose how displayed results are rounded when formatting to a finite number
    of digits. The default is :menuselection:`Half Away from Zero (Arithmetic)`.
    Available modes are:
    :menuselection:`Half Even (Banker's)`,
    :menuselection:`Half Away from Zero (Arithmetic)`,
    :menuselection:`Up (Ceil)`,
    :menuselection:`Down (Floor)`,
    :menuselection:`Toward Zero (Truncation)`.

    * :menuselection:`Half Even (Banker's)`
        Round to nearest; ties go to the nearest even last kept digit.
        Example at 1 fractional digit: ``2.250 -> 2.2``,
        ``2.350 -> 2.4``, ``-2.250 -> -2.2``.
    * :menuselection:`Half Away from Zero (Arithmetic)`
        Round to nearest; ties increase magnitude.
        Example at 1 fractional digit: ``2.250 -> 2.3``,
        ``-2.250 -> -2.3``.
    * :menuselection:`Toward Zero`
        Discard extra digits without rounding up.
        Example at 1 fractional digit: ``2.290 -> 2.2``,
        ``-2.290 -> -2.2``.
    * :menuselection:`Toward +Infinity`
        Always round upward.
        Example at 1 fractional digit: ``2.210 -> 2.3``,
        ``-2.210 -> -2.2``.
    * :menuselection:`Toward -Infinity`
        Always round downward.
        Example at 1 fractional digit: ``2.290 -> 2.2``,
        ``-2.210 -> -2.3``.

    This setting affects result formatting (including live previews). It does
    not change the behavior of the :func:`round` and :func:`trunc` functions.
    Like other result-format settings, changes apply to subsequent calculations
    and previews; already stored history entries keep their original display.
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
    immediately; :guilabel:`Apply` applies the selected or customized theme
    without closing the dialog.

    The :guilabel:`Preview` area shows representative result-display and
    editor content so that the syntax-highlighting, result, background,
    editor-background, and scrollbar roles are visible while editing.

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
    ``cursor``, ``number``, ``parens``, ``list``, ``unit``, ``result``, ``comment``, ``matched``,
    ``function``, ``operator``, ``variable``, ``scrollbar``, ``separator``,
    ``background``, ``editorbackground``.
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
* :kbd:`Control+L`
    Load session.
* :kbd:`Control+S`
    Save session.
* :kbd:`Control+Q`
    Quit SpeedCrunch.
* :kbd:`Control+N`
    Clear history.
* :kbd:`Control+C`
    Copy selected text to clipboard.
* :kbd:`Control+R`
    Copy last result to clipboard.
* :kbd:`Control+V`
    Paste from clipboard.
* :kbd:`Control+A`
    Select entire expression.
* :kbd:`Control+(` or :kbd:`Control+)`
    Wrap the current selection in parentheses. If no text is selected, the entire expression is wrapped.

Widgets and Docks
+++++++++++++++++

* :kbd:`Control+1`
    Show/hide formula book.
* :kbd:`Control+2`
    Show/hide constants widget.
* :kbd:`Control+3`
    Show/hide functions widgets.
* :kbd:`Control+4`
    Show/hide variables widget.
* :kbd:`Control+5`
    Show/hide user functions widget.
* :kbd:`Control+6`
    Show/hide bit field dock.
* :kbd:`Control+7`
    Show/hide history widget.
* :kbd:`Control+8`
    Show/hide user units widget.
* :kbd:`Control+B`
    Show/hide the status bar.
    The status bar provides quick selectors for :menuselection:`Angle Mode`
    and :menuselection:`Results --> Notation`. When
    :menuselection:`Settings --> Complex Numbers` is enabled, it also shows
    a :menuselection:`Complex Form` selector with the same options as
    :menuselection:`Notation & Precision...`. On narrow windows, selectors
    that do not fully fit are hidden automatically.

Scrolling
+++++++++

* :kbd:`Page Up` and :kbd:`Page Down`
    Scroll the result window page-wise.
* :kbd:`Shift+Page Up` and :kbd:`Shift+Page Down`
    Scroll the result window line-wise.
* :kbd:`Control+Page Up` and :kbd:`Control+Page Down`
    Scroll to the top or bottom of the result window.


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
* :kbd:`F6`
    Set result notation to binary.
* :kbd:`F7`
    Set result notation to octal.
* :kbd:`F8`
    Set result notation to hexadecimal.
* :kbd:`F9`
    Set result notation to sexagesimal.

    .. versionadded:: 1.0
    
Various
+++++++

* :kbd:`F1`
    Show context help (dismiss with :kbd:`Escape`).

    .. versionadded:: 0.12

* :kbd:`F11`
    Toggle full screen.
* :kbd:`Control` + mouse wheel, :kbd:`Shift` + mouse wheel, or :kbd:`Shift+Up` and :kbd:`Shift+Down`
    Change the font size.
* :kbd:`Control+Shift` + mouse wheel
    Change the window opacity.

    .. versionadded:: 0.12


.. rubric:: Footnotes

.. [#f1] Starting with SpeedCrunch 0.12, the session format is based on `JSON <json_>`_. Previous
         versions used a simple custom text format.

.. _json: http://json.org/
