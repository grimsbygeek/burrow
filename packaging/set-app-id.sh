#!/bin/sh
# Changes the application ID everywhere it appears. Usage: packaging/set-app-id.sh org.example.MyApp
set -eu
new="${1:?usage: $0 <new-app-id>}"
case "$new" in
    *[!A-Za-z0-9._-]*|.*|*.|*..*|"") echo "not a valid app ID: $new" >&2; exit 1 ;;
esac
case "$new" in *.*.*) ;; *) echo "an app ID needs at least three parts, like com.example.App" >&2; exit 1 ;; esac

cd "$(dirname "$0")/.."
old=$(sed -n 's/^app-id: //p' packaging/flatpak/*.yml | head -1)
[ -n "$old" ] || { echo "cannot find the current app ID" >&2; exit 1; }

old_re=$(printf '%s' "$old" | sed 's/[.[\*^$/]/\\&/g')
sed -i "s/$old_re/$new/g" packaging/flatpak/"$old".yml CMakeLists.txt
[ "packaging/flatpak/$old.yml" = "packaging/flatpak/$new.yml" ] || mv "packaging/flatpak/$old.yml" "packaging/flatpak/$new.yml"
echo "app ID: $old -> $new"
