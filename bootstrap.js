const setTimeout = (cb, timeout) => {
  return _setTimeout([cb, timeout]);
}

const setInterval = (cb, interval) => {
  return _setInterval([cb, interval]);
}

RUNTIME_PATH = 'runtime/';


console = {
  log: (arg) => {
    _log(arg);
  }
}

const requireCache = {};

require = (file) => {
  if (requireCache[file]){
    return requireCache[file];
  } else {
    const requiredModule = _require(RUNTIME_PATH + file + '.js');
    requireCache[file] = requiredModule.exports;
    return requireCache[file];
  }
}

VERSION = 'JS Runtime 0.0.1';