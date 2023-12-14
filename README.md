This repo is based off of: https://github.com/NicolasDe/AlienSwarm

Portal-Swarm is Portal 2007's code and HL2 2013's code ported to Alien Swarm.

There are no extra fixes made, the fixes that are made are to make something functional during the porting process.

Some fixes did require me to look at the leaked CSGO code for reference in order to see how Portal 2 got around certain issues.

Unfortunately, it doesn't seem possible to mount the Portal VPKs or HL2 VPKs directly. SFM is needed for the hl2 content, but for Portal you'll need to dump everything from portal_pak_dir.vpk to the pak01_dir folder, you won't need to turn this into a VPK, but do not replace any of the files because they are needed for Swarm compatibility.


Credits:

https://github.com/stanriders/hl2-asw-port - GameUI Code & Other Ideas

https://github.com/RubberWar/Portal-2 - Some shader stuff