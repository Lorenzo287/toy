; @keyword: Usually Purple/Pink. (Commonly used for def, return).
; @keyword.control: Usually Purple/Pink (often italicized). (For if, while).
; @string.special: Usually Cyan/Light Blue. (Used for escape characters or regex).
; @constant: Usually Orange/Red. (For hardcoded constants).
; @constant.builtin: Often Red. (For values like nil, null, true, false).
; @type: Usually Yellow/Aqua. (For class names or types).
; @type.builtin: Often Yellow. (For int, float, string).
; @tag: Usually Red/Pink/Green. (From HTML, but very distinct in most themes).
; @label: Usually Blue/Cyan. (Used for loop labels or jump targets).
; @namespace: Usually Yellow/Aqua. (For modules or packages).
; @attribute: Usually Blue/Cyan. (Used for decorators/annotations).
; @comment.note: Often Cyan/Blue (bold).
; @comment.error: Often Red (bold).
; @punctuation.special: Usually Red/Orange.

(line_comment) @comment
(block_comment) @comment

(number) @number
(boolean) @boolean
(string) @string
(escape_sequence) @string.escape

;; Symbols and user words form Toy's neutral baseline. Definition sites receive
;; the function color below.
(quoted_symbol) @variable

(source_file
  (quoted_symbol) @function
  .
  (block)
  .
  (builtin_word) @_def
  (#eq? @_def "def")
  (#set! "priority" 110))

(block
  (quoted_symbol) @function
  .
  (block)
  .
  (builtin_word) @_def
  (#eq? @_def "def")
  (#set! "priority" 110))

;; Variables and Parameters
((var_list (word) @variable.parameter)
 (#set! "priority" 110))
((var_fetch) @variable.parameter
 (#set! "priority" 110))

;; Capture delimiters behave like quotation/collection brackets.
(var_list "|" @punctuation.bracket)

;; Brackets
(block "[" @punctuation.bracket "]" @punctuation.bracket)
(list_literal "(" @punctuation.bracket ")" @punctuation.bracket)
(map_literal "{" @punctuation.bracket "}" @punctuation.bracket)
(set_literal "#{" @punctuation.bracket "}" @punctuation.bracket)

(control_word) @keyword.control
(operator) @operator
(builtin_word) @function.builtin

;; Zero-argument words that always push the same numeric value.
((builtin_word) @constant
 (#any-of? @constant "pi" "e" "tau" "inf" "nan")
 (#set! "priority" 110))

;; Word 'def' specifically as keyword
((builtin_word) @keyword
 (#eq? @keyword "def")
 (#set! "priority" 110))

;; User words share the neutral baseline with ordinary symbols.
(word) @variable
