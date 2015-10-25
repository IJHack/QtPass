#FAQ

## Issues

### Can't save a password
* Is folder initialised? Easiest way is to use the [Users] button and make sure you can encrypt for someone (eg. yourself)
* Are you using git? If not, make sure it is switched off.

### I have an issue with GNOME keyring
* Disable GNOME keyring
* Create a `~/.gnupg/gpg-agent.conf` containing:
```
enable-ssh-support
write-env-file
use-standard-socket
default-cache-ttl 600
max-cache-ttl 7200
```

Also, the following is useful to add to your .bashrc if you are using Yubikey NEO on Ubuntu:

```
# OpenPGP applet support for YubiKey NEO
if [ ! -f /tmp/gpg-agent.env ]; then
    killall gpg-agent;
        eval $(gpg-agent --daemon --enable-ssh-support > /tmp/gpg-agent.env);
fi
. /tmp/gpg-agent.env
```

* More info: [issue 60](https://github.com/IJHack/qtpass/issues/60) and [issue 73](https://github.com/IJHack/qtpass/issues/73)

### Where can I ask for help?
* Create an [issue](https://github.com/IJHack/qtpass/) issues on github.
* Send an email to [help@qtpass.org](help@qtpass.org)

### Can I import from KeePass, LastPass or X?
* Yes, check [passwordstore.org/#migration](http://www.passwordstore.org/#migration) for more info.

### I don't like the design, what gives?
* It's all on github, clone, change and send a pull request.
* Open an issue and point out defects or better yet propose changes.

## How can I help improve QtPass?


###I would like to donate!

* Time:
  * Fork, clone hack and send a pull request.
  * Find an [issue](https://github.com/IJHack/qtpass/issues) to work on..
  * Participate in our bug bounty, you submit an issue and help us fix it, I send you a bounty.
* Money:
IJhack takes donations in [bitcoin](https://blockchain.info/address/146dqz8zXn9iNZMv5s7JVqwZKjrmumHBfb)
