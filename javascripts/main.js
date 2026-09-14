document.addEventListener('DOMContentLoaded', function () {
  var config = document.getElementById('config');
  var qtpass = document.getElementById('qtpass');

  if (qtpass && config) {
    qtpass.addEventListener('click', function () {
      qtpass.classList.add('hidden');
      config.classList.remove('hidden');
    });

    config.addEventListener('click', function () {
      config.classList.add('hidden');
      qtpass.classList.remove('hidden');
    });
  }

  if ('serviceWorker' in navigator) {
    navigator.serviceWorker.register('/sw.js');
  }
});