# Tesseract OCR (Anzen Fork)

[Original repo](https://github.com/tesseract-ocr/tesseract)

This is anzen's fork of `tesseract`. It is largely unchanged from the original repository, with the primary differences being:
- This extracts the table information that tesseract identifies internally that is currently not exposed externally
- We have our own github actions workflow for building and publishing self-contained builds that are used in [anzen-platform](https://github.com/goanzen/anzen-platform)

## Building

```bash
./autogen.sh  # only needs to be run once unless you change something in `Makefile.am`
./build.sh
```

The built files can be found in `build-out`.

## Releasing

1. Create a tag prefixed with `anzen-v...` (e.g. `git tag anzen-v9.9.9`). You can use `git tag | grep anzen` to find the existing tags; you should increment from the latest one.
2. Push the tag to github (`git push <tag>`)
3. You should see a release created in github with builds for all requested platforms (see `.github/workflows/release.yaml` for targetting platforms).
