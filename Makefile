subdirs := loop2spmd

all:
	lit subdirs

clean:
	for sd in $subdirs; do ${MAKE} clean -d $${sd}; done;