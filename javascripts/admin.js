// Easter egg: type a password off the office whiteboard (Welkom01, Welcome02,
// see main.js) and you are logged in to the admin panel of a site with no
// server. Everything here is fake and stays in this page: no request, no
// storage, no field that takes input. The panel's password audit is a typing
// game: encrypt the passwords before they reach the store.
(function () {
  var CSS = "/stylesheets/admin.css?v=1.8.4-2";
  var calm = window.matchMedia("(prefers-reduced-motion: reduce)").matches;
  var dialog = null;
  var game = null;
  var timers = [];

  var after = function (ms, fn) {
    var id = setTimeout(fn, calm ? Math.min(ms, 60) : ms);
    timers.push(id);
    return id;
  };

  // h("p.note", "text", child, ...)
  var h = function (spec) {
    var parts = spec.split(".");
    var el = document.createElement(parts[0]);
    if (parts.length > 1) {
      el.className = parts.slice(1).join(" ");
    }
    Array.prototype.slice.call(arguments, 1).forEach(function (kid) {
      el.append(kid);
    });
    return el;
  };
  var button = function (spec, label, fn) {
    var el = h("button." + spec, label);
    el.type = "button";
    el.addEventListener("click", fn);
    return el;
  };
  var table = function (head, rows) {
    return h(
      "table",
      h(
        "thead",
        h.apply(
          null,
          ["tr"].concat(
            head.map(function (cell) {
              return h("th", cell);
            }),
          ),
        ),
      ),
      h.apply(
        null,
        ["tbody"].concat(
          rows.map(function (row) {
            return h.apply(
              null,
              ["tr"].concat(
                row.map(function (cell) {
                  return h("td", cell);
                }),
              ),
            );
          }),
        ),
      ),
    );
  };

  var rotate = function (word) {
    return word.replace(/\d+$/, function (digits) {
      return String(Number(digits) + 1).padStart(digits.length, "0");
    });
  };

  // ------------------------------------------------------------- the panel

  var users = [
    ["admin", "Welkom01", "2014"],
    ["backup_admin", "Welkom02", "2014"],
    ["ceo", "Geheim01", "never"],
    ["intern", "Welcome01", "first day"],
    ["printer", "Admin123", "2009"],
    ["root", "*******", "all we see is stars"],
  ];
  var rotations = 0;

  var PAGES = {
    Dashboard: function (word) {
      return [
        h("h2", "Welkom, admin!"),
        h(
          "div.admin-stats",
          stat("Visitors online", "1", "that's you"),
          stat("Databases", "0", "we looked"),
          stat("Server-side code", "0 lines", "it's HTML"),
          stat("Stored passwords", "0", "they're on your machine"),
          stat("Last login", "now", "with " + word),
          stat("Security level", "Beige", "up from Taupe"),
        ),
        h(
          "div.admin-alert",
          h("strong", "1 audit overdue. "),
          "The auditors found the post-it.",
        ),
        button("admin-audit", "Run password audit", startAudit),
      ];
    },
    Users: function () {
      var box = h("div");
      var draw = function () {
        box.replaceChildren(
          table(["User", "Password", "Last changed"], users),
          h(
            "p",
            rotations
              ? "Rotated " +
                  rotations +
                  (rotations === 1 ? " time" : " times") +
                  ". Compliance achieved."
              : "Policy: change every 90 days. Plus one counts.",
          ),
          button("", "Rotate all passwords", function () {
            rotations += 1;
            users.forEach(function (row) {
              row[1] = rotate(row[1]);
              if (row[0] !== "root") {
                row[2] = "just now";
              }
            });
            draw();
          }),
        );
      };
      draw();
      return [h("h2", "Users"), box];
    },
    Passwords: function () {
      return [
        h("h2", "Customer passwords"),
        h("p", "0 records found."),
        h(
          "p",
          "qtpass.org is a folder of HTML. Your passwords are GnuPG-encrypted " +
            "files on your own machines, and this panel can't reach them. " +
            "It can't reach anything.",
        ),
      ];
    },
    Database: function () {
      var out = h("pre", "mysql> SHOW TABLES;\nEmpty set (0.00 sec)");
      return [
        h("h2", "Database"),
        out,
        button("", "DROP TABLE users", function () {
          out.textContent +=
            "\nmysql> DROP TABLE users;\nQuery OK, 0 rows affected" +
            "\nThere is no mysql. You typed that into a <pre>.";
        }),
      ];
    },
    Backups: function () {
      var note = h("p", "Next backup: when someone remembers.");
      return [
        h("h2", "Backups"),
        table(
          ["File", "Size", "Stored on"],
          [
            ["backup_final.zip", "0 bytes", "this server"],
            ["backup_final2.zip", "0 bytes", "this server"],
            ["backup_final2_echt.zip", "0 bytes", "the intern's USB stick"],
          ],
        ),
        note,
        button("", "Restore everything", function () {
          note.textContent = "Restored 0 bytes. Nothing lost. Nothing gained.";
        }),
      ];
    },
    Logs: function () {
      var LINES = [
        "GET /wp-admin 404 (not WordPress)",
        "GET /.env 404 (nice try)",
        "POST /xmlrpc.php 405",
        "GET /phpmyadmin 404 (see: Database)",
        "GET /.git/config 404",
        "GET /admin 404 (you're in the wrong admin)",
        "GET /shell.php 404",
        "login: admin, Welkom01, OK",
        "login: admin, Welkom01, OK",
        "login: admin, Welkom01, OK (from 14 countries)",
      ];
      var log = h("pre.admin-log");
      var at = 0;
      var tick = function () {
        if (!log.isConnected) {
          return;
        }
        var line = LINES[at % LINES.length];
        at += 1;
        log.textContent = (log.textContent + line + "\n")
          .split("\n")
          .slice(-14)
          .join("\n");
        after(900, tick);
      };
      after(0, tick);
      return [h("h2", "Logs"), log];
    },
    Settings: function () {
      var note = h("p", "");
      var refuse = function (text) {
        return button("", text, function () {
          note.textContent = "Your administrator has disabled this setting.";
        });
      };
      return [
        h("h2", "Settings"),
        h(
          "div.admin-row",
          refuse("Enable security"),
          refuse("Make the site dynamic"),
          refuse("Force password change"),
        ),
        note,
      ];
    },
  };

  function stat(label, value, note) {
    return h("div.admin-stat", h("span", label), h("b", value), h("em", note));
  }

  var showPanel = function (word) {
    var main = h("div.admin-main");
    var nav = h("div.admin-nav");
    var open = function (name) {
      main.replaceChildren.apply(main, PAGES[name](word));
      nav.querySelectorAll("button").forEach(function (b) {
        b.classList.toggle("on", b.textContent === name);
      });
    };
    Object.keys(PAGES).forEach(function (name) {
      nav.append(
        button("", name, function () {
          open(name);
        }),
      );
    });
    nav.append(
      button("", "Password audit", startAudit),
      button("", "Log out", function () {
        dialog.close();
      }),
    );
    dialog.replaceChildren(
      h(
        "div.admin-panel",
        h(
          "div.admin-bar",
          h("strong", "QtPass Site Beheer"),
          h("span", "v0.1 · logged in as admin"),
        ),
        h("div.admin-body", nav, main),
        h(
          "div.admin-foot",
          "This admin is fake. qtpass.org is static HTML: no login, no " +
            "server code, no passwords. Esc to leave.",
        ),
      ),
    );
    open("Dashboard");
  };

  var showLogin = function (word) {
    var user = h("output.admin-field", "");
    var pass = h("output.admin-field", "");
    var status = h("p.admin-status", "");
    var go = h("button", "Log in");
    go.type = "button";
    go.disabled = true;
    dialog.replaceChildren(
      h(
        "div.admin-login",
        h("h2", "QtPass Site Beheer"),
        h("label", "Username"),
        user,
        h("label", "Password"),
        pass,
        go,
        status,
      ),
    );
    var name = "admin";
    var fill = function (i) {
      if (i <= name.length) {
        user.textContent = name.slice(0, i);
        after(70, function () {
          fill(i + 1);
        });
        return;
      }
      var dots = function (j) {
        if (j <= word.length) {
          pass.textContent = "•".repeat(j);
          after(50, function () {
            dots(j + 1);
          });
          return;
        }
        status.textContent = "Checking against the post-it...";
        after(900, function () {
          status.textContent = "Correct. Password last changed: 2014.";
          after(900, function () {
            showPanel(word);
          });
        });
      };
      dots(1);
    };
    fill(1);
  };

  // ------------------------------------------------------------- the audit

  var OFFICE = [
    "Welkom01",
    "Welcome01",
    "Welkom123",
    "Zomer2024",
    "Winter2023",
    "Lente2025",
    "Herfst2022",
    "Wachtwoord1",
    "Password1",
    "Qwerty123",
    "Bedrijf123",
    "Admin123",
    "Test1234",
    "Geheim01",
    "P@ssw0rd",
    "Changeme1",
    "Letmein1",
    "Fietsen1",
    "Koffie123",
    "Maandag1",
    "Vrijdag13",
    "Sinterklaas1",
    "Printer1",
    "Stagiair01",
    "Iloveyou1",
    "Dragon123",
    "Monkey12",
    "Football1",
  ];
  var WAVES = [
    "Q1: mandatory rotation",
    "Q2: the interns arrive",
    "Q3: audit week",
    "Q4: Friday deploy",
    "Summer: everyone's on holiday",
    "Merger: two post-its",
    "Compliance: plus one counts",
  ];
  var LIVES = 5;
  var CIPHER =
    "ABCDEFGHIJKLMNOPQRSTUVWXYZabcdefghijklmnopqrstuvwxyz0123456789+/";
  var FONT = 'ui-monospace, "SFMono-Regular", Menlo, Consolas, monospace';

  function Audit(root, onExit) {
    var canvas = document.createElement("canvas");
    var ctx = canvas.getContext("2d");
    var muteBtn = button("admin-mute", "Sound on", function () {
      muted = !muted;
      muteBtn.textContent = muted ? "Sound off" : "Sound on";
      muteBtn.blur();
    });
    root.replaceChildren(canvas, muteBtn);
    var W = 0;
    var H = 0;
    var size = 20;
    var fit = function () {
      var dpr = window.devicePixelRatio || 1;
      W = root.clientWidth;
      H = root.clientHeight;
      canvas.width = W * dpr;
      canvas.height = H * dpr;
      ctx.setTransform(dpr, 0, 0, dpr, 0, 0);
      size = Math.max(16, Math.min(24, W / 45));
    };
    fit();
    window.addEventListener("resize", fit);

    var audio = null;
    var muted = false;
    try {
      audio = new (window.AudioContext || window.webkitAudioContext)();
    } catch (e) {
      audio = null;
    }
    var tone = function (freq, length, type, gain, delay) {
      if (!audio || muted) {
        return;
      }
      var t = audio.currentTime + (delay || 0);
      var osc = audio.createOscillator();
      var vol = audio.createGain();
      osc.type = type || "square";
      osc.frequency.value = freq;
      vol.gain.setValueAtTime(gain || 0.03, t);
      vol.gain.exponentialRampToValueAtTime(0.0001, t + length);
      osc.connect(vol).connect(audio.destination);
      osc.start(t);
      osc.stop(t + length);
    };

    var words = [];
    var bits = [];
    var queue = [];
    var lock = null;
    var score = 0;
    var combo = 0;
    var best = 0;
    var bombs = 1;
    var nextBomb = 1500;
    var wave = 0;
    var leaks = 0;
    var encrypted = 0;
    var typed = 0;
    var missed = 0;
    var leaked = {};
    var banner = "";
    var bannerT = 0;
    var flash = 0;
    var shake = 0;
    var spawnT = 0;
    var last = performance.now();
    var running = true;
    var raf = 0;

    var shuffle = function (list) {
      var out = list.slice();
      for (var i = out.length - 1; i > 0; i -= 1) {
        var j = Math.floor(Math.random() * (i + 1));
        var keep = out[i];
        out[i] = out[j];
        out[j] = keep;
      }
      return out;
    };

    var startWave = function () {
      wave += 1;
      var boss = wave % 4 === 0;
      var count = 7 + wave * 2;
      queue = shuffle(OFFICE).slice(0, count);
      if (wave >= 2) {
        queue.splice(3, 0, "hunter2");
      }
      if (boss) {
        queue.push("CVE-2017-18021");
      }
      banner = boss
        ? "Boss: the one in the changelog"
        : WAVES[(wave - 1) % WAVES.length];
      bannerT = 2.2;
      spawnT = 2.2;
      tone(330, 0.15, "triangle", 0.05);
      tone(494, 0.2, "triangle", 0.05, 0.14);
    };

    var floor = function () {
      return H - 54;
    };
    var spawn = function () {
      // A free first letter, so one key can only mean one word.
      var i = queue.findIndex(function (word) {
        return !words.some(function (w) {
          return w.text[0].toLowerCase() === word[0].toLowerCase();
        });
      });
      if (i < 0) {
        return;
      }
      var text = queue.splice(i, 1)[0];
      ctx.font = "600 " + size + "px " + FONT;
      var width = ctx.measureText(text).width;
      var boss = text.indexOf("CVE") === 0;
      var fall = Math.max(3.6, 10.5 - wave * 0.8) * (boss ? 2 : 1);
      words.push({
        text: text,
        lower: text.toLowerCase(),
        hide: text === "hunter2",
        boss: boss,
        at: 0,
        x: 24 + Math.random() * Math.max(10, W - width - 48),
        y: 52,
        speed: (floor() - 52) / fall,
      });
    };

    var burst = function (word) {
      ctx.font = "600 " + size + "px " + FONT;
      var cw = ctx.measureText("M").width;
      for (var i = 0; i < word.text.length; i += 1) {
        for (var n = 0; n < 3; n += 1) {
          bits.push({
            c: CIPHER[Math.floor(Math.random() * CIPHER.length)],
            x: word.x + i * cw,
            y: word.y,
            vx: (Math.random() - 0.5) * 240,
            vy: -60 - Math.random() * 200,
            life: 0.7 + Math.random() * 0.5,
          });
        }
      }
    };

    var remove = function (word) {
      words.splice(words.indexOf(word), 1);
      if (lock === word) {
        lock = null;
      }
    };

    var encrypt = function (word) {
      burst(word);
      remove(word);
      encrypted += 1;
      combo += 1;
      best = Math.max(best, combo);
      score += word.text.length * 10 * (1 + Math.floor(combo / 5));
      if (score >= nextBomb) {
        bombs += 1;
        nextBomb += 1500;
        tone(880, 0.3, "triangle", 0.05, 0.1);
      }
      tone(523, 0.07);
      tone(784, 0.1, "square", 0.03, 0.06);
    };

    var leak = function (word) {
      var hits = word.boss ? 3 : 1;
      leaks += hits;
      combo = 0;
      leaked[word.text] = (leaked[word.text] || 0) + 1;
      remove(word);
      flash = 1;
      shake = calm ? 0 : 12;
      tone(110, 0.35, "sawtooth", 0.06);
      // The fix for a leaked password: add one.
      if (!word.boss && !word.hide) {
        queue.unshift(rotate(word.text));
      }
      if (leaks >= LIVES) {
        finish();
      }
    };

    var miss = function () {
      missed += 1;
      combo = 0;
      shake = calm ? 0 : Math.max(shake, 3);
      tone(160, 0.08, "square", 0.025);
    };

    var key = function (raw) {
      var c = raw.toLowerCase();
      typed += 1;
      if (!lock) {
        lock =
          words
            .filter(function (w) {
              return w.lower[0] === c;
            })
            .sort(function (a, b) {
              return b.y - a.y;
            })[0] || null;
        if (!lock) {
          miss();
          return;
        }
        lock.at = 1;
      } else if (lock.lower[lock.at] === c) {
        lock.at += 1;
      } else {
        miss();
        return;
      }
      tone(320 + lock.at * 30, 0.03, "square", 0.015);
      if (lock.at === lock.text.length) {
        encrypt(lock);
      }
    };

    var bomb = function () {
      if (!bombs || !words.length) {
        return;
      }
      bombs -= 1;
      flash = 0.6;
      banner = "pass generate";
      bannerT = 1;
      words.slice().forEach(encrypt);
      tone(196, 0.5, "sawtooth", 0.05);
    };

    var draw = function () {
      ctx.save();
      ctx.clearRect(0, 0, W, H);
      ctx.fillStyle = "#0b1420";
      ctx.fillRect(0, 0, W, H);
      if (shake) {
        ctx.translate(
          (Math.random() - 0.5) * shake,
          (Math.random() - 0.5) * shake,
        );
      }
      ctx.textBaseline = "top";

      // the store, at the bottom
      ctx.fillStyle = "#16324f";
      ctx.fillRect(0, floor(), W, H - floor());
      ctx.fillStyle = "#6ea8e8";
      ctx.fillRect(0, floor(), W, 2);
      ctx.font = "600 15px " + FONT;
      ctx.fillStyle = "#cfe0f2";
      ctx.fillText("~/.password-store", 16, floor() + 18);
      var integrity =
        "integrity " +
        "#".repeat(Math.max(LIVES - leaks, 0)) +
        ".".repeat(Math.min(leaks, LIVES));
      ctx.fillText(integrity, W / 2 - 80, floor() + 18);
      var hint = "[Enter] pass generate x" + bombs;
      ctx.fillText(hint, W - ctx.measureText(hint).width - 16, floor() + 18);

      // the score
      ctx.fillStyle = "#cfe0f2";
      ctx.fillText("score " + score, 16, 16);
      ctx.fillText("wave " + wave, 150, 16);
      if (combo >= 5) {
        ctx.fillStyle = "#ffd479";
        ctx.fillText("x" + (1 + Math.floor(combo / 5)), 250, 16);
      }

      // the falling passwords
      ctx.font = "600 " + size + "px " + FONT;
      words.forEach(function (w) {
        var shown = w.hide
          ? w.text.slice(0, w.at) + "*".repeat(w.text.length - w.at)
          : w.text;
        var done = shown.slice(0, w.at);
        var w0 = ctx.measureText(done).width;
        if (w === lock) {
          ctx.fillStyle = "rgba(110, 168, 232, 0.18)";
          ctx.fillRect(
            w.x - 6,
            w.y - 4,
            ctx.measureText(shown).width + 12,
            size + 8,
          );
        }
        ctx.fillStyle = "#74e0a8";
        ctx.fillText(done, w.x, w.y);
        var close = (w.y - 52) / (floor() - 52);
        ctx.fillStyle = w.boss
          ? "#ff8a7a"
          : close > 0.75
            ? "#ffb37a"
            : "#f1f4f7";
        ctx.fillText(shown.slice(w.at), w.x + w0, w.y);
      });

      // the ciphertext they turn into
      ctx.font = "600 14px " + FONT;
      bits.forEach(function (b) {
        ctx.globalAlpha = Math.min(1, b.life * 2);
        ctx.fillStyle = "#74e0a8";
        ctx.fillText(b.c, b.x, b.y);
      });
      ctx.globalAlpha = 1;

      if (flash) {
        ctx.fillStyle = "rgba(255, 90, 70, " + flash * 0.3 + ")";
        ctx.fillRect(0, 0, W, H);
      }
      if (bannerT > 0) {
        ctx.textAlign = "center";
        ctx.font = "700 " + Math.round(size * 1.4) + "px " + FONT;
        ctx.fillStyle = "#ffd479";
        ctx.globalAlpha = Math.min(1, bannerT);
        ctx.fillText(banner, W / 2, H * 0.3);
        ctx.globalAlpha = 1;
        ctx.textAlign = "left";
      }
      ctx.restore();
    };

    var frame = function (now) {
      if (!running) {
        return;
      }
      var dt = Math.min((now - last) / 1000, 0.05);
      last = now;
      bannerT = Math.max(0, bannerT - dt);
      flash = Math.max(0, flash - dt * 2);
      shake = Math.max(0, shake - dt * 30);
      spawnT -= dt;
      if (spawnT <= 0 && queue.length) {
        spawn();
        spawnT = Math.max(0.7, 2.3 - wave * 0.18);
      }
      words.slice().forEach(function (w) {
        w.y += w.speed * dt;
        if (w.y + size >= floor() && running) {
          leak(w);
        }
      });
      bits = bits.filter(function (b) {
        b.x += b.vx * dt;
        b.y += b.vy * dt;
        b.vy += 420 * dt;
        b.life -= dt;
        return b.life > 0;
      });
      if (running && !queue.length && !words.length) {
        startWave();
      }
      draw();
      if (running) {
        raf = requestAnimationFrame(frame);
      }
    };

    var grade = function () {
      if (encrypted >= 60) {
        return ["B-", "Best grade available. There's still a post-it."];
      }
      if (encrypted >= 30) {
        return ["C", "Acceptable, said nobody."];
      }
      if (encrypted >= 12) {
        return ["D", "Your auditor would like a word. It's Welkom03."];
      }
      return ["F", "The printer has better security."];
    };

    function finish() {
      running = false;
      cancelAnimationFrame(raf);
      tone(196, 0.3, "sawtooth", 0.05);
      tone(147, 0.3, "sawtooth", 0.05, 0.25);
      tone(98, 0.6, "sawtooth", 0.05, 0.5);
      var worst =
        Object.keys(leaked).sort(function (a, b) {
          return leaked[b] - leaked[a];
        })[0] || "none";
      var result = grade();
      var again = button("", "Audit again", function () {
        stop();
        game = new Audit(root, onExit);
      });
      root.append(
        h(
          "div.admin-report",
          h("h2", "Password audit report"),
          h("p.admin-grade", result[0]),
          h("p", result[1]),
          table(
            ["Finding", "Result"],
            [
              ["Passwords encrypted", String(encrypted)],
              ["Leaked", String(Object.keys(leaked).length)],
              ["Worst offender", worst],
              ["Best streak", String(best)],
              ["Typos", String(missed)],
              ["Score", String(score)],
              ["Waves survived", String(wave - 1)],
            ],
          ),
          h(
            "p",
            "Recommendation: let a machine pick the passwords. We know a " +
              "tool for that.",
          ),
          h(
            "div.admin-row",
            again,
            button("", "Back to admin", function () {
              onExit();
            }),
          ),
        ),
      );
      again.focus();
    }

    function stop() {
      running = false;
      cancelAnimationFrame(raf);
      window.removeEventListener("resize", fit);
      if (audio) {
        audio.close();
      }
    }

    this.stop = stop;
    this.onKey = function (event) {
      if (!running) {
        return false;
      }
      if (event.key === "Enter") {
        bomb();
        return true;
      }
      if (event.key === "Backspace") {
        if (lock) {
          lock.at = 0;
          lock = null;
        }
        return true;
      }
      if (
        event.key.length === 1 &&
        !event.ctrlKey &&
        !event.metaKey &&
        !event.altKey
      ) {
        key(event.key);
        return true;
      }
      return false;
    };

    startWave();
    raf = requestAnimationFrame(frame);
  }

  var startAudit = function () {
    var panel = dialog.querySelector(".admin-panel");
    var begin = function () {
      var root = h("div.admin-game");
      dialog.replaceChildren(root);
      game = new Audit(root, leaveAudit);
    };
    if (calm || !panel) {
      begin();
      return;
    }
    // The panel was held together with sticky tape.
    panel
      .querySelectorAll(
        ".admin-bar, .admin-nav button, .admin-main > *, .admin-stat, .admin-foot",
      )
      .forEach(function (part) {
        part.classList.add("admin-falling");
        part.style.setProperty("--spin", (Math.random() - 0.5) * 70 + "deg");
        part.style.setProperty("--drift", (Math.random() - 0.5) * 240 + "px");
        part.style.setProperty("--wait", Math.random() * 0.35 + "s");
      });
    after(1100, begin);
  };

  function leaveAudit() {
    if (game) {
      game.stop();
      game = null;
    }
    showPanel("Welkom01");
  }

  window.qtpassAdmin = function (word) {
    if (dialog) {
      return;
    }
    dialog = h("dialog.admin");
    dialog.setAttribute("aria-label", "QtPass Site Beheer, a fake admin panel");
    dialog.addEventListener("cancel", function (event) {
      if (game) {
        event.preventDefault();
        leaveAudit();
      }
    });
    dialog.addEventListener("close", function () {
      if (game) {
        game.stop();
        game = null;
      }
      timers.forEach(clearTimeout);
      timers = [];
      dialog.remove();
      dialog = null;
    });
    document.body.appendChild(dialog);
    dialog.showModal();
    showLogin(word);
  };

  // While the audit runs, the keyboard belongs to the game.
  window.addEventListener(
    "keydown",
    function (event) {
      if (game && game.onKey(event)) {
        event.preventDefault();
        event.stopPropagation();
      }
    },
    true,
  );

  var link = document.createElement("link");
  link.rel = "stylesheet";
  link.href = CSS;
  document.head.appendChild(link);
})();
