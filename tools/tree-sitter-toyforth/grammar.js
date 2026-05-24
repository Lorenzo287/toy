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
      $.block_comment,
    )),

    _expression: $ => choice(
      $.number,
      $.boolean,
      $.string,
      $.quoted_symbol,
      $.var_fetch,
      $.block,
      $.var_list,
      $.colon_definition,
      $.control_word,
      $.operator,
      $.builtin_word,
      $.word,
    ),

    line_comment: $ => token(seq('\\', /.*/)),
    block_comment: $ => token(seq('(', /[^)]*/, ')')),
    boolean: $ => choice('true', 'false'),
    number: $ => token(choice(
      /\d+\.\d+/,  // float first
      /\d+/,
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
      'typeof', 'bool?', 'int?', 'float?', 'str?', 'symbol?', 'list?', 'number?', 'nan?', 'inf?',
      'word?', 'var?', 'inf', 'nan', 'body', 'intern', 'name', 'words', 'see',
      'geth', 'seth', 'slice', 'take', 'dropn', 'len', 'first', 'rest', 'uncons', 'cons',
      'append', 'concat', 'join', 'trim', 'upper', 'lower', 'splitmid', 'range', 'empty?',
      'rand', 'sleep', 'argc', 'argv', 'getenv', 'setenv', 'pwd', 'shell', 'time', 'clock',
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
      repeat(choice($._expression, $.line_comment, $.block_comment)),
      ']',
    ),
    // fix 1: allow any expression inside {}, mirroring the lexer
    var_list: $ => seq(
      '{',
      repeat(choice($._expression, $.line_comment, $.block_comment)),
      '}',
    ),
    // fix 2: use dedicated _def_name rule instead of aliasing $.word
    colon_definition: $ => seq(
      ':',
      alias($._def_name, $.definition_name),
      repeat(choice($._expression, $.line_comment, $.block_comment)),
      ';',
    ),
    _def_name: $ => token(/[a-zA-Z0-9_+\-*/%<>=!.?]+/),

    word: $ => /[a-zA-Z0-9_+\-*/%<>=!.?]+/,
  }
});
