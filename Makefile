.PHONY: build serve docs docs-check site

build:
	python3 tools/build.py

docs:
	python3 tools/build_docs.py

site: build docs

serve: site
	python3 -m http.server 5501 --directory _site

docs-check:
	python3 tools/build_docs.py --check
