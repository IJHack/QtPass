document.addEventListener('DOMContentLoaded', function () {
  var config = document.getElementById('config');
  var qtpass = document.getElementById('qtpass');

  if (qtpass && config) {
    qtpass.addEventListener('click', function () {
      config.classList.toggle('hidden');
    });

    config.addEventListener('click', function () {
      config.classList.add('hidden');
    });
  }

  if ('serviceWorker' in navigator) {
    navigator.serviceWorker.register('/sw.js');
  }
});