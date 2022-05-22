console.log('[Timers] Initialized')
setTimeout(() => {
  console.log('[Timers] Timer has run after 1000 ms');
}, 1000);

setInterval(() => {
  console.log('[Timers] Interval callback has been invoked');
}, 10000);