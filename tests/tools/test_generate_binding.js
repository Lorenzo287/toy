const assert = require('assert');
const {
  renderBinding,
  validateManifest,
} = require('../../tools/generate-binding.js');

function manifest() {
  return {
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
assert(rendered.includes('*toy_module_init('));
assert(rendered.includes('toy_module_bind(abi_version, api)'));
assert(!rendered.includes('TOY_MODULE_ABI_VERSION'));
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

function resourceManifest() {
  return {
    module: 'sample.generated',
    headers: ['sample.h'],
    resources: [
      {
        name: 'handle',
        cType: 'sample_handle *',
        destructor: 'sample_handle_destroy',
      },
    ],
    functions: [
      {
        name: 'sample_handle_new',
        word: 'make-handle',
        returns: {resource: 'handle'},
        args: ['i64'],
      },
      {
        name: 'sample_handle_value',
        word: 'handle-value',
        returns: 'i64',
        args: [{resource: 'handle'}],
      },
      {
        name: 'sample_handle_open',
        word: 'open-handle',
        returns: {status: 'i32', success: [0, 1]},
        args: ['i64', {outResource: 'handle'}],
      },
    ],
  };
}

const resourceRendered = renderBinding(validateManifest(resourceManifest()));
assert(resourceRendered.includes('"sample.generated.handle"'));
assert(resourceRendered.includes('sample_handle_destroy(value)'));
assert(resourceRendered.includes('toy_get_resource(state, 0'));
assert(resourceRendered.includes('sample_handle_open(argument_0, &argument_1)'));
assert(resourceRendered.includes('result == (int32_t)0 || result == (int32_t)1'));
assert(resourceRendered.includes('binding_destroy_resource_0(argument_1, NULL)'));

const duplicateResource = resourceManifest();
duplicateResource.resources.push({...duplicateResource.resources[0]});
assert.throws(
  () => validateManifest(duplicateResource),
  /duplicate resource/
);

const unsafeCType = resourceManifest();
unsafeCType.resources[0].cType = 'sample_handle *; int injected';
assert.throws(() => validateManifest(unsafeCType), /C pointer type/);

const unknownResource = resourceManifest();
unknownResource.functions[0].returns.resource = 'missing';
assert.throws(() => validateManifest(unknownResource), /unknown resource/);

const scalarWithOutput = resourceManifest();
scalarWithOutput.functions[2].returns = 'i32';
assert.throws(
  () => validateManifest(scalarWithOutput),
  /output resources require void or status returns/
);

const duplicateSuccess = resourceManifest();
duplicateSuccess.functions[2].returns.success = [0, 0];
assert.throws(() => validateManifest(duplicateSuccess), /duplicate code/);

const outOfRangeSuccess = resourceManifest();
outOfRangeSuccess.functions[2].returns = {status: 'u8', success: [256]};
assert.throws(() => validateManifest(outOfRangeSuccess), /outside u8's range/);

const tooManyOutputs = resourceManifest();
tooManyOutputs.functions[2].args.push({outResource: 'handle'});
assert.throws(() => validateManifest(tooManyOutputs), /at most one output/);
