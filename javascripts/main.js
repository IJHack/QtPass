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

  // Platform capture slots: the app screenshots for Windows, macOS and
  // FreeBSD are not captured yet, so an absent image falls back to the
  // platform badge instead of rendering a broken icon. Add the captures
  // (windows-app(-dark).png, macos-app(-dark).png, freebsd-app(-dark).png)
  // and the fallback disappears on its own.
  document.querySelectorAll(".capture").forEach(function (capture) {
    var shot = capture.querySelector("img.shot");
    if (!shot) {
      return;
    }
    var broken = function () {
      shot.classList.add("broken");
    };
    if (shot.complete && shot.naturalWidth === 0) {
      broken();
    } else {
      shot.addEventListener("error", broken);
    }
  });

  // Five quick clicks on the logo unlock it (javascripts/unlock.js, loaded
  // only then).
  var clicks = 0;
  var resetClicks;
  document.querySelectorAll("img.mark").forEach(function (mark) {
    mark.addEventListener("click", function () {
      clicks += 1;
      clearTimeout(resetClicks);
      resetClicks = setTimeout(function () {
        clicks = 0;
      }, 1500);
      if (clicks < 5) {
        return;
      }
      clicks = 0;
      if (window.qtpassUnlock) {
        window.qtpassUnlock();
        return;
      }
      var script = document.createElement("script");
      script.src = "/javascripts/unlock.js?v=1.8.2-5";
      script.onload = function () {
        window.qtpassUnlock();
      };
      document.head.appendChild(script);
    });
  });

  // The 404 page: the padlock shakes its head, as at a wrong password; click
  // it and it does so again.
  if (document.getElementById("404")) {
    document.querySelectorAll("img.mark").forEach(function (mark) {
      var nope = function () {
        mark.classList.remove("nope");
        void mark.offsetWidth;
        mark.classList.add("nope");
      };
      nope();
      mark.addEventListener("click", nope);
    });
  }

  // Type hunter2 anywhere and all we see is stars (bash.org #244321).
  var typed = "";
  document.addEventListener("keydown", function (event) {
    if (event.key.length !== 1 || event.ctrlKey || event.metaKey || event.altKey) {
      return;
    }
    if (event.target.closest && event.target.closest("input, textarea")) {
      return;
    }
    typed = (typed + event.key.toLowerCase()).slice(-7);
    if (typed !== "hunter2" || document.querySelector(".hunter2")) {
      return;
    }
    var toast = document.createElement("p");
    toast.className = "hunter2";
    toast.setAttribute("role", "status");
    var stars = document.createElement("strong");
    stars.textContent = "*******";
    var note = document.createElement("span");
    note.textContent = "hunter2? All we see is stars.";
    toast.append(stars, note);
    document.body.appendChild(toast);
    setTimeout(function () {
      toast.remove();
    }, 3500);
  });

  if ("serviceWorker" in navigator) {
    navigator.serviceWorker.register("/sw.js");
  }
});
