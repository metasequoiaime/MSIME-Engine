const assert = require('node:assert/strict');
const fs = require('node:fs');
const protocol = require('./runtime.js');
const cases = JSON.parse(fs.readFileSync(__dirname + '/fixtures.json', 'utf8'));
for (const test of cases) {
  assert.equal(protocol.validate(test.message, test.direction, test.surface), test.valid, test.name);
}
assert.deepEqual(JSON.parse(protocol.serializeClientMessage({type: 'configRequest'}, 'settings')),
  {type: 'configRequest', protocolVersion: 1});
assert.throws(() => protocol.serializeClientMessage({type: 'candidate', data: 0}, 'candidate'), TypeError);
assert.throws(() => protocol.serializeClientMessage({type: 'configUpdate', data: {path: 'input.mode', value: NaN}}, 'settings'), TypeError);
assert.equal(protocol.validate({type: '__proto__'}, 'client', 'settings'), false);
console.log(`${cases.length} shared fixtures and serializer checks passed`);
