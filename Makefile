IMAGE = tfpcmcia
DOCKER = PATH="/Applications/Docker.app/Contents/Resources/bin:$$PATH" docker

.PHONY: all clean flash docker-image

all: docker-image
	$(DOCKER) run --rm -v $(PWD):/host $(IMAGE) make -j -C /host/firmware

clean:
	$(DOCKER) run --rm -v $(PWD):/host $(IMAGE) make -C /host/firmware clean

flash:
	minichlink -w firmware/tfpcmcia.bin flash -b

docker-image:
	$(DOCKER) build -q -t $(IMAGE) . --platform linux/amd64
