# Sonic Unleashed Recompiled - PlayStation 5 Native Port (PPSA99902)
#
# Preserves the upstream CMake build (CMakeLists.txt / CMakePresets.json) for
# Windows, Linux, and macOS while adding native PS5 (__PROSPERO__) build,
# automatic retail XEX + Xenos shader recompilation from ./ressources/{game,update,dlc},
# FSELF signing, FTP deployment, and ps5-homebrew-dev-protocol launch/close targets.

SHELL := /bin/bash
.DEFAULT_GOAL := app

-include .env

TITLE_ID ?= PPSA99902
PS5_HOST ?=
FTP_PORT ?= 2121
PS5_ELF_PORT ?= 9021
RESSOURCES_DIR ?= ressources

.PHONY: all app build deps tools assets recomp deploy undeploy launch close clean

all: app
build: app

# Bootstrap PS5 Payload SDK, Mesa RADV (libvulkan_radeon.ps5.a), PacBrew ports, and host tools
deps: tools
	@bash tools/setup-native-dependencies.sh
	@bash tools/build-radv.sh release
	@bash tools/setup-pacbrew-dependencies.sh --all >/dev/null

# Build host recompiler and asset tools (XenonRecomp, XenosRecomp, x_decompress, file_to_c, png_to_bc7_dds, wav_to_at9)
tools:
	@bash tools/build-host-tools.sh

# Auto-convert sce_sys/pic0.png & pic1.png -> pic0.dds & pic1.dds, sce_sys/snd0.{wav,mp3,ogg,flac} -> snd0.at9, and validate sce_sys/
assets:
	@bash tools/prepare-assets.sh

# Run XenonRecomp + x_decompress + XenosRecomp on ./ressources/{game,update,dlc}
recomp:
	@bash tools/recomp-xex.sh "$(RESSOURCES_DIR)"

# Full PS5 build: auto-runs recomp if ./ressources/game is populated, compiles, links, and signs eboot.bin
app:
	@bash tools/build.sh

# Deploy dist/PPSA99902 to PS5 over FTP (/data/homebrew/PPSA99902)
deploy: app
	@TITLE_ID=$(TITLE_ID) PS5_HOST=$(PS5_HOST) FTP_PORT=$(FTP_PORT) bash tools/deploy.sh

undeploy:
	@TITLE_ID=$(TITLE_ID) PS5_HOST=$(PS5_HOST) FTP_PORT=$(FTP_PORT) bash tools/deploy.sh undeploy

# Launch / Close PPSA99902 on PS5 via ps5-homebrew-dev-protocol
launch:
	@PS5_PAYLOAD_SDK="$${PS5_PAYLOAD_SDK:-$(abspath .deps/native/ps5-payload-sdk)}" \
		bash tools/send-controller.sh launch "$(TITLE_ID)" "$(PS5_HOST)" "$(PS5_ELF_PORT)"

close:
	@PS5_PAYLOAD_SDK="$${PS5_PAYLOAD_SDK:-$(abspath .deps/native/ps5-payload-sdk)}" \
		bash tools/send-controller.sh close "$(TITLE_ID)" "$(PS5_HOST)" "$(PS5_ELF_PORT)"

clean:
	@rm -rf build dist pkg
