'''
    This script is used to generate the .ino file version of this project.
    It is generated to ease people without platform io to still tinker with
    this project.

    - Royyan
'''

# Importing the modules
import os
import shutil

start_dir = os.getcwd() #get the current working dir
src_dir = start_dir+"/src" #get the current working dir
print(start_dir)

# set dest_dir
dest_dir = start_dir+"/jarvis-light-switch-ino"

# check if path exists
if (not os.path.isdir(dest_dir)):
    # create a dir where we want to copy and rename
    dest_dir = os.mkdir('jarvis-light-switch-ino')
    dest_dir = start_dir+"/jarvis-light-switch-ino"

src_file = os.path.join(src_dir, 'main.cpp')
shutil.copy(src_file,dest_dir) #copy the file to destination dir

dst_file = os.path.join(dest_dir,'main.cpp')
new_dst_file_name = os.path.join(dest_dir, 'jarvis-light-switch-ino.ino')

os.rename(dst_file, new_dst_file_name)#rename
os.chdir(dest_dir)

print(os.listdir())