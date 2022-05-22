 const { existsSync } = require('lib/fs');
console.log('[Fs-test] test.js exists: ' + existsSync('test.js'));
console.log('[Fs-test] missing.js exists: ' + existsSync('missing.js'));