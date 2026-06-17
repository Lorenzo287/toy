/**
 * @file Toyforth grammar for tree-sitter
 * @author Lorenzo287
 * @license MIT
 */

export default grammar({
  name: 'toyforth',
  extras: $ => [
    /\s/,
  ],
  rules: {
    source_file: $ => repeat(choice(
      $._expression,
      $.line_comment,
    )),

    _expression: $ => choice(
      $.number,
      $.boolean,
      $.string,
      $.quoted_symbol,
      $.var_fetch,
      $.block,
      $.list_literal,
      $.map_literal,
      $.set_literal,
      $.var_list,
      $.colon_definition,
      $.control_word,
      $.operator,
      $.builtin_word,
      $.word,
    ),

    line_comment: $ => token(seq('\\', /.*/)),
    boolean: $ => choice('true', 'false'),
    number: $ => token(choice(
      /[+-]?\d+\.\d*/,  // float first
      /[+-]?\.\d+/,
      /[+-]?\d+/,
    )),
    control_word: $ => choice(
      'if', 'ifelse', 'while', 'try', 'error', 'exec', 'i', 'app2',
      'infra', 'cond', 'cleave', 'construct', 'replicate', 'times',
      'dip', 'keep', 'bi', 'linrec', 'binrec', 'genrec', 'treerec',
      'each', 'map', 'fold', 'filter', 'some', 'all', 'split', 'merge'
    ),
    operator: $ => choice(
      '+', '-', '*', '/', '%', 'mod', 'abs', 'neg', 'max', 'min',
      'sqrt', 'pow', 'exp', 'log', 'log10', 'sin', 'cos', 'tan',
      'floor', 'ceil', 'round', 'pred', 'succ', 'square', 'cube',
      'and', 'or', 'xor', 'not', 'shl', 'shr',
      '==', '!=', '<', '>', '<=', '>='
    ),
    builtin_word: $ => choice(
      'dup', 'drop', 'swap', 'over', 'rot', 'swapd', 'nip', 'tuck', 'pick', 'roll',
      'empty',
      'pi', 'e', 'tau',
      'print', 'printf', '.', '.s', '.S', 'cr',
      'key', 'input', 'load', 'readf', 'writef', 'delf', 'readl', 'exists?', 'clear', 'page',
      'typeof', 'bool?', 'int?', 'float?', 'string?', 'symbol?', 'vector?', 'list?', 'number?', 'sequence?', 'callable?', 'nan?', 'inf?',
      'map?', 'set?', 'deque?', 'pqueue?',
      'word?', 'var?', 'inf', 'nan', 'body', 'intern', 'name', 'words', 'see',
      '>vector', '>list', '>map', '>set', '>deque', '>pqueue', 'contains?', 'indexof', 'unique', 'sort',
      'has?', 'get', 'assoc', 'dissoc', 'keys', 'values', 'pairs', 'items', 'adjoin', 'remove',
      'push-front', 'push-back', 'pop-front', 'pop-back', 'front', 'back',
      'pqueue-push', 'pqueue-peek', 'pqueue-pop', 'pqueue-drain',
      'at', 'set-at', 'slice', 'take', 'dropn', 'len', 'first', 'rest', 'uncons', 'cons',
      'append', 'concat', 'reverse', 'join', 'trim', 'upper', 'lower', 'splitmid', 'range', 'empty?',
      'char?', 'letter?', 'digit?', 'alnum?', 'space?', 'upper?', 'lower?', 'punct?',
      'rand', 'sleep', 'argc', 'argv', 'env?', 'getenv', 'setenv', 'pwd', 'shell', 'time', 'clock',
      'def', 'bye', 'exit'
    ),
    string: $ => seq(
      '"',
      repeat(choice(
        $._string_content,
        $.escape_sequence,
      )),
      '"',
    ),
    _string_content: $ => token.immediate(/[^"\\]+/),
    escape_sequence: $ => token.immediate(choice(
      /\\n/,
      /\\r/,
      /\\t/,
      /\\"/,
      /\\\\/,
      /\\033/,
      /\\0/,
      /\\./,  // catch-all
    )),
    quoted_symbol: $ => seq(
      "'",
      alias(/[a-zA-Z0-9_+\-*/%<>=!.?]+/, $.symbol_name)
    ),
    var_fetch: $ => seq(
      '$',
      alias(/[a-zA-Z0-9_+\-*/%<>=!.?]+/, $.variable_name)
    ),
    block: $ => seq(
      '[',
      repeat(choice($._expression, $.line_comment)),
      ']',
    ),
    list_literal: $ => seq(
      '(',
      repeat(choice($._expression, $.line_comment)),
      ')',
    ),
    map_literal: $ => seq(
      '{',
      repeat(choice($._expression, $.line_comment)),
      '}',
    ),
    set_literal: $ => seq(
      '#{',
      repeat(choice($._expression, $.line_comment)),
      '}',
    ),
    // Captures bind names from the data stack inside the current frame.
    var_list: $ => seq(
      '|',
      repeat1($.word),
      '|',
    ),
    // fix 2: use dedicated _def_name rule instead of aliasing $.word
    colon_definition: $ => seq(
      ':',
      alias($._def_name, $.definition_name),
      repeat(choice($._expression, $.line_comment)),
      ';',
    ),
    _def_name: $ => token(/[a-zA-Z_+\-*/%<>=!.?][a-zA-Z0-9_+\-*/%<>=!.?]*/),

    word: $ => /[a-zA-Z_+\-*/%<>=!.?][a-zA-Z0-9_+\-*/%<>=!.?]*/,
  }
});
