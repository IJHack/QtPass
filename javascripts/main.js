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
      script.src = "/javascripts/unlock.js?v=1.8.3-12";
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
    }, 6500);
    if (window.matchMedia("(prefers-reduced-motion: reduce)").matches) {
      return;
    }
    // On the second heartbeat the keyhole flashes and padlocks pour out of
    // it for a few seconds: a fountain with gravity, each its own size,
    // spin and depth.
    setTimeout(burst, 2550);
  });

  function burst() {
    // The watermark sits at right -12rem, top -6rem, 46rem square; the
    // keyhole is at about (50%, 68%) of the logo.
    var rem =
      parseFloat(getComputedStyle(document.documentElement).fontSize) || 16;
    var size = 46 * rem;
    var originX = window.innerWidth + 12 * rem - size / 2;
    var originY = -6 * rem + size * 0.68;
    var flash = document.createElement("div");
    flash.className = "konami-flash";
    flash.style.left = originX + "px";
    flash.style.top = originY + "px";
    flash.addEventListener("animationend", function () {
      flash.remove();
    });
    document.body.appendChild(flash);

    var drops = [];
    for (var i = 0; i < 110; i += 1) {
      var depth = 0.35 + Math.random() * 0.65; // 1 is close, small is far
      var drop = document.createElement("img");
      drop.src = "/images/logo.svg";
      drop.alt = "";
      drop.className = "konami-drop";
      var px = Math.round(10 + depth * 34);
      drop.style.width = drop.style.height = px + "px";
      drop.style.filter =
        depth < 0.6 ? "blur(" + ((0.6 - depth) * 4).toFixed(1) + "px)" : "";
      document.body.appendChild(drop);
      var angle = (-90 + (Math.random() * 150 - 95)) * (Math.PI / 180); // mostly up and to the left
      var speed = (420 + Math.random() * 520) * depth;
      drops.push({
        el: drop,
        half: px / 2,
        x: originX,
        y: originY,
        vx: Math.cos(angle) * speed,
        vy: Math.sin(angle) * speed,
        spin: (Math.random() * 2 - 1) * 540,
        turn: 0,
        // a stream from the keyhole: heaviest at first, tapering off
        delay: Math.pow(Math.random(), 1.8) * 2.6,
        depth: depth,
      });
    }
    var start = null;
    var last = null;
    var gravity = 1500;
    function frame(now) {
      if (start === null) {
        start = last = now;
      }
      var dt = Math.min(0.05, (now - last) / 1000);
      last = now;
      var t = (now - start) / 1000;
      var alive = 0;
      drops.forEach(function (d) {
        if (!d.el) {
          return;
        }
        if (t < d.delay) {
          d.el.style.opacity = "0";
          alive += 1;
          return;
        }
        d.vy += gravity * d.depth * dt;
        d.vx *= 1 - 0.6 * dt;
        d.x += d.vx * dt;
        d.y += d.vy * dt;
        d.turn += d.spin * dt;
        var age = t - d.delay;
        var fadeIn = Math.min(1, age / 0.12);
        var fadeOut = Math.max(
          0,
          Math.min(1, (window.innerHeight - d.y) / (window.innerHeight * 0.35)),
        );
        d.el.style.opacity = (
          fadeIn *
          fadeOut *
          (0.55 + 0.45 * d.depth)
        ).toFixed(3);
        d.el.style.transform =
          "translate(" +
          (d.x - d.half) +
          "px," +
          (d.y - d.half) +
          "px) rotate(" +
          d.turn.toFixed(1) +
          "deg)";
        if (d.y - d.half > window.innerHeight || age > 5) {
          d.el.remove();
          d.el = null;
          return;
        }
        alive += 1;
      });
      if (alive) {
        requestAnimationFrame(frame);
      }
    }
    requestAnimationFrame(frame);
  }

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

  // The screenshots page can show its pictures in either scheme: the dark
  // <source>s get media "all" (dark) or "not all" (light), and their own
  // media query back for the reader's scheme.
  var schemeSwitch = document.querySelector(".shot-scheme");
  if (schemeSwitch) {
    var schemeScope = schemeSwitch.closest(".release") || document;
    var darkSources = Array.prototype.slice.call(
      schemeScope.querySelectorAll(
        'source[media*="prefers-color-scheme: dark"]',
      ),
    );
    darkSources.forEach(function (source) {
      source.dataset.media = source.media;
    });
    schemeSwitch.addEventListener("click", function (event) {
      var picked = event.target.closest("button[data-scheme]");
      if (!picked) {
        return;
      }
      var scheme = picked.dataset.scheme;
      schemeScope.dataset.scheme = scheme;
      darkSources.forEach(function (source) {
        source.media =
          scheme === "dark"
            ? "all"
            : scheme === "light"
              ? "not all"
              : source.dataset.media;
      });
      schemeSwitch.querySelectorAll("button").forEach(function (b) {
        b.setAttribute("aria-pressed", String(b === picked));
      });
    });
  }

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
        // A link to the dark capture itself starts in dark, a picked
        // scheme on the screenshots page wins over the reader's own.
        var picked = link.closest("[data-scheme]");
        showDark =
          Boolean(link.dataset.light) ||
          (picked && picked.dataset.scheme
            ? picked.dataset.scheme === "dark"
            : dark.matches);
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
