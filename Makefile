# Compose name of OOMMF tar ball as available on NIST webpages
# (as of September 2015)
OOMMFPREFIX="oommf"
OOMMFVERSION="12a5bis"
OOMMFDATE="20120928"
OOMMFROOTNAME=$(OOMMFPREFIX)$(OOMMFVERSION)
OOMMFSOURCEFILE=$(OOMMFROOTNAME)_$(OOMMFDATE).tar.gz


all:
	make get-oommf
	make untar-oommf
	make update-logfile
	echo "Run git 'commit -a' or similar to update git repo with new release"

get-oommf:
	mkdir -p Downloads
	cd Downloads && wget http://math.nist.gov/oommf/dist/$(OOMMFSOURCEFILE)

untar-oommf:
	tar xfvz Downloads/$(OOMMFSOURCEFILE)
	# next is a manual hack, may need to update for future releases
	mv $(OOMMFPREFIX)-1.2a5bis $(OOMMFPREFIX)

update-logfile:
	echo "`date`: unpacked $(OOMMFSOURCEFILE)" >> versionlog.txt


