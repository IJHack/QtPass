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
      script.src = "/javascripts/unlock.js?v=1.8.3-7";
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

  // The Konami code wakes the padlock in the background: it comes forward,
  // flips like a coin, beats twice, and small padlocks rain from it. With
  // reduced motion it only glows for a moment.
  var konami = [
    "ArrowUp",
    "ArrowUp",
    "ArrowDown",
    "ArrowDown",
    "ArrowLeft",
    "ArrowRight",
    "ArrowLeft",
    "ArrowRight",
    "b",
    "a",
  ];
  var konamiAt = 0;
  var backdrop = document.querySelector(".background");
  document.addEventListener("keydown", function (event) {
    if (
      !backdrop ||
      event.target.closest("input, textarea, [contenteditable]")
    ) {
      return;
    }
    var key = event.key.length === 1 ? event.key.toLowerCase() : event.key;
    konamiAt =
      key === konami[konamiAt] ? konamiAt + 1 : key === konami[0] ? 1 : 0;
    if (konamiAt < konami.length) {
      return;
    }
    konamiAt = 0;
    if (backdrop.classList.contains("awake")) {
      return;
    }
    backdrop.classList.add("awake");
    setTimeout(function () {
      backdrop.classList.remove("awake");
    }, 4200);
    if (window.matchMedia("(prefers-reduced-motion: reduce)").matches) {
      return;
    }
    var box = backdrop.getBoundingClientRect();
    var fromX = window.innerWidth - 12 * 16 - box.left;
    for (var i = 0; i < 28; i += 1) {
      var drop = document.createElement("img");
      drop.src = "/images/logo.svg";
      drop.alt = "";
      drop.className = "konami-drop";
      drop.style.left = Math.round(fromX - 40 + Math.random() * 380) + "px";
      drop.style.setProperty(
        "--drift",
        Math.round(Math.random() * 240 - 120) + "px",
      );
      drop.style.setProperty(
        "--spin",
        Math.round(Math.random() * 720 - 360) + "deg",
      );
      drop.style.animationDelay = (0.9 + Math.random() * 1.4).toFixed(2) + "s";
      drop.style.width = drop.style.height =
        14 + Math.round(Math.random() * 22) + "px";
      drop.addEventListener("animationend", function () {
        this.remove();
      });
      document.body.appendChild(drop);
    }
  });

  // Type hunter2 anywhere and all we see is stars (bash.org #244321).
  var typed = "";
  document.addEventListener("keydown", function (event) {
    if (
      event.key.length !== 1 ||
      event.ctrlKey ||
      event.metaKey ||
      event.altKey
    ) {
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

  // Screenshots open full size in a lightbox instead of a bare image
  // page; the links still point at the PNG, so they work without script.
  // Arrows (and the arrow keys) go through the page's screenshots; where a
  // capture exists in light and dark (data-dark / data-light on the link),
  // the moon/sun button (and "t") switches, starting from the reader's own
  // colour scheme.
  var fullSizeLinks = Array.prototype.slice.call(
    document.querySelectorAll('a[title="Full size"], a.full'),
  );
  if (fullSizeLinks.length && typeof HTMLDialogElement === "function") {
    var dark = window.matchMedia("(prefers-color-scheme: dark)");
    var box = document.createElement("dialog");
    box.className = "lightbox";
    var big = document.createElement("img");
    big.decoding = "async";
    var button = function (cls, label, text) {
      var b = document.createElement("button");
      b.type = "button";
      b.className = "lightbox-button " + cls;
      b.setAttribute("aria-label", label);
      b.title = label;
      b.textContent = text;
      return b;
    };
    var closeButton = button("lightbox-close", "Close", "×");
    var prevButton = button("lightbox-prev", "Previous screenshot", "‹");
    var nextButton = button("lightbox-next", "Next screenshot", "›");
    var themeButton = button("lightbox-theme", "Switch light and dark", "◐");
    box.append(closeButton, prevButton, nextButton, themeButton, big);
    document.body.appendChild(box);

    var current = 0;
    var showDark = false;
    var variants = function (link) {
      var href = link.getAttribute("href");
      return {
        light: link.dataset.light || href,
        dark: link.dataset.dark || (link.dataset.light ? href : null),
      };
    };
    var show = function () {
      var link = fullSizeLinks[current];
      var v = variants(link);
      var hasBoth = Boolean(v.dark && v.light !== v.dark);
      themeButton.hidden = !hasBoth;
      // Show where the button goes: a moon to dark, a sun to light.
      themeButton.textContent = showDark ? "\u2600" : "\u263e";
      themeButton.setAttribute(
        "aria-label",
        showDark ? "Show the light version" : "Show the dark version",
      );
      themeButton.title = themeButton.getAttribute("aria-label");
      var src = hasBoth && showDark ? v.dark : v.light;
      if (!hasBoth) {
        src = link.getAttribute("href");
      }
      var thumb = link.querySelector("img");
      big.alt = thumb ? thumb.alt : "";
      box.setAttribute("aria-label", big.alt || "Screenshot");
      big.src = src;
      var many = fullSizeLinks.length > 1;
      prevButton.hidden = !many;
      nextButton.hidden = !many;
    };
    var step = function (delta) {
      current = (current + delta + fullSizeLinks.length) % fullSizeLinks.length;
      show();
    };
    var toggleTheme = function () {
      if (!themeButton.hidden) {
        showDark = !showDark;
        show();
      }
    };
    prevButton.addEventListener("click", function (event) {
      event.stopPropagation();
      step(-1);
    });
    nextButton.addEventListener("click", function (event) {
      event.stopPropagation();
      step(1);
    });
    themeButton.addEventListener("click", function (event) {
      event.stopPropagation();
      toggleTheme();
    });
    box.addEventListener("click", function () {
      box.close();
    });
    box.addEventListener("keydown", function (event) {
      if (event.key === "ArrowLeft") {
        step(-1);
      } else if (event.key === "ArrowRight") {
        step(1);
      } else if (event.key === "t" || event.key === "T") {
        toggleTheme();
      } else {
        return;
      }
      event.preventDefault();
    });
    box.addEventListener("close", function () {
      big.removeAttribute("src");
    });
    fullSizeLinks.forEach(function (link, index) {
      link.addEventListener("click", function (event) {
        if (event.ctrlKey || event.metaKey || event.shiftKey || event.button) {
          return; // a new tab or window was asked for
        }
        event.preventDefault();
        current = index;
        // A link to the dark capture itself starts in dark.
        showDark = Boolean(link.dataset.light) || dark.matches;
        show();
        box.showModal();
        closeButton.focus();
      });
    });
  }

  if ("serviceWorker" in navigator) {
    navigator.serviceWorker.register("/sw.js");
  }
});
