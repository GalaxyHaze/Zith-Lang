.PHONY: build serve docs docs-check

build:
	python3 tools/build.py

serve: build
	python3 -m http.server 5501 --directory _site

docs:
	python3 tools/build_docs.py

docs-check:
	python3 tools/build_docs.py --check
