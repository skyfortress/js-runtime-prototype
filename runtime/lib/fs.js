module.exports = {
  existsSync: (path) => {
    return _existsSync(RUNTIME_PATH + path) === 0;
  }
}