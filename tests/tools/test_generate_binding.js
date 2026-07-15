const assert = require('assert');
const {
  renderBinding,
  validateManifest,
} = require('../../tools/generate-binding.js');

function manifest() {
  return {
    schemaVersion: 1,
    module: 'sample.generated',
    headers: ['sample.h'],
    functions: [
      {
        name: 'sample_length',
        word: 'length',
        returns: 'usize',
        args: ['cstr'],
      },
    ],
  };
}

const rendered = renderBinding(validateManifest(manifest()));
assert(rendered.includes('"sample.generated"'));
assert(rendered.includes('{"length", binding_word_0}'));
assert(rendered.includes('sample_length(argument_0)'));
assert(rendered.includes("memchr(string_data_0, '\\0', string_length_0)"));
assert(!rendered.includes('\0'));

const badHeader = manifest();
badHeader.headers = ['sample.h"\n#error injected'];
assert.throws(() => validateManifest(badHeader), /safe include path/);

const voidArgument = manifest();
voidArgument.functions[0].args = ['void'];
assert.throws(() => validateManifest(voidArgument), /cannot be void/);

const duplicateWord = manifest();
duplicateWord.functions.push({
  name: 'sample_other',
  word: 'length',
  returns: 'void',
  args: [],
});
assert.throws(() => validateManifest(duplicateWord), /duplicate Toy word/);

const unknownField = manifest();
unknownField.functions[0].return = 'usize';
assert.throws(() => validateManifest(unknownField), /not supported/);
