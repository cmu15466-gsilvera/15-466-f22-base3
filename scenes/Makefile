.PHONY : all

#n.b. the '-y' sets autoexec scripts to 'on' so that driver expressions will work
UNAME_S := $(shell uname -s)
ifeq ($(UNAME_S),Darwin)
	BLENDER = /Applications/Blender.app/Contents/MacOS/Blender -y
else ifeq ($(UNAME_S),Linux)
	BLENDER = ../../blender-2.93.4-linux-x64/blender -y
else
	BLENDER = blender
endif

EXPORT_MESHES=export-meshes.py
EXPORT_SCENE=export-scene.py

DIST=../dist

all : \
	$(DIST)/scene_op \
	$(DIST)/pnct_op \

INPUTFILE=world

$(DIST)/scene_op :
	$(BLENDER) --background --python $(EXPORT_SCENE) -- $(INPUTFILE).blend $(DIST)/$(INPUTFILE).scene

$(DIST)/pnct_op :
	$(BLENDER) --background --python $(EXPORT_MESHES) -- $(INPUTFILE).blend $(DIST)/$(INPUTFILE).pnct
