
SOURCE:=$(shell find modules) antora.yml local-playbook.yml mrdocs.cpp mrdocs.yml package-lock.json

.PHONY: build
build : build/lib/doc/msm/index.html

.PHONY: setup
setup:
	npm install
	wget -q https://github.com/cppalliance/mrdocs/releases/download/v0.8.0/MrDocs-0.8.0-Linux.tar.xz -O mrdocs.tar.xz
	sudo tar -xJf mrdocs.tar.xz -C /usr/local --strip-components=1 MrDocs-0.8.0-Linux
	rm mrdocs.tar.xz

build/lib/doc/msm/index.html: $(SOURCE)
	npx antora --fetch local-playbook.yml --stacktrace
