document.addEventListener("DOMContentLoaded", function () {
  var config = document.getElementById("config");
  var qtpass = document.getElementById("qtpass");

  if (qtpass && config) {
    qtpass.addEventListener("click", function () {
      config.classList.toggle("hidden");
    });

    config.addEventListener("click", function () {
      config.classList.add("hidden");
    });
  }

  // Pin the sidebar only when the whole of it fits beside the content.
  var sidebar = document.querySelector(".sidebar");
  if (sidebar) {
    var fitSidebar = function () {
      sidebar.classList.remove("flows");
      var top = parseFloat(getComputedStyle(sidebar).top) || 0;
      if (sidebar.offsetHeight + top > window.innerHeight) {
        sidebar.classList.add("flows");
      }
    };
    fitSidebar();
    window.addEventListener("load", fitSidebar);
    window.addEventListener("resize", fitSidebar);
  }

  // Packaging badges come from repology.org. When that host is down (it
  // was, see IJHack/QtPass#1824) a broken image with a long alt text is
  // worse than nothing, so drop the badge link when the image fails.
  document
    .querySelectorAll('a[href^="https://repology.org/"] > img')
    .forEach(function (img) {
      var drop = function () {
        img.parentNode.hidden = true;
      };
      if (img.complete && img.naturalWidth === 0) {
        drop();
      } else {
        img.addEventListener("error", drop);
      }
    });

  if ("serviceWorker" in navigator) {
    navigator.serviceWorker.register("/sw.js");
  }
});
