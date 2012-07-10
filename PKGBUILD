# Maintainer: Roland Kammerer <dev.rck@gmail.com>
pkgname=dush-git
pkgver=20120710
pkgrel=1
pkgdesc="find and display largest files/directories"
arch=('i686' 'x86_64')
url="https://github.com/rck/dush"
license=('GPL3')
groups=()
depends=()
makedepends=('git' 'cmake' 'uthash')
provides=()
conflicts=(dush)
replaces=()
backup=()
options=()
install=
source=()
noextract=()
md5sums=() #generate with 'makepkg -g'

_gitroot=git://github.com/rck/dush.git
_gitname=dush

build() {
  cd "$srcdir"
  msg "Connecting to GIT server...."

  if [[ -d "$_gitname" ]]; then
    cd "$_gitname" && git pull origin
    msg "The local files are updated."
  else
    git clone "$_gitroot" "$_gitname"
  fi

  msg "GIT checkout done or server timeout"
  msg "Starting build..."

  rm -rf "$srcdir/$_gitname-build"
  git clone "$srcdir/$_gitname" "$srcdir/$_gitname-build"
  cd "$srcdir/$_gitname-build"

  #
  # BUILD HERE
  #
  cmake -DCMAKE_INSTALL_PREFIX=/usr
  make
}

package() {
  cd "$srcdir/$_gitname-build"
  make DESTDIR="$pkgdir/" install
}

# vim:set ts=2 sw=2 et:
