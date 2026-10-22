#!/bin/sh
set -eu
cd "$(dirname "$0")/.."
# Uses the maintainer's existing Netlify CLI login. No credentials are stored here.
npx --yes netlify-cli@27.14.2 deploy --prod --no-build --dir website \
  --site ed0ff908-6d18-4b18-aea3-ace20c73a845 \
  --message "DCE public demo"
