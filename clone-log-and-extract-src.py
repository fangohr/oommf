doc = """
This programm will
(i) clone a git repo (which is an OOMMF extension) with name REPONAME
(ii) copy from a particular folder in that direcotry (typically 'src') 
     all files into the oommf/app/oxs/local/REPONAME folder
(iii) create a provenance-REPONAME-HEAD.zip folder that contains a zipped
     version of the git repo HEAD at the time of the copy
(iv) store some additional information about that extension repository
     in provenance-REPONAME.log 

See helpstring and source code for details.
"""


import os
import subprocess
import sys
import time


def main(argv):
    repo_url = argv[1]
    copy_from = sys.argv[2] + "/"

    assert repo_url.endswith('.git')
    repo_name = os.path.split(repo_url)[1].split('.git')[0]
    print(f"Processing {repo_name}, copy from {copy_from}")
    subprocess.check_call(f"git clone {repo_url} tmp_git_clone", shell=True)

    # log what repository and version these files are from
    filename = f'provenance-{repo_name}.log'
    
    # create a zip file of the HEAD of the repository (the version we use).
    # This way, additional documentation and examples
    # are available if desired:
    zipfilename = f'provenance-{repo_name}-HEAD.zip'
    subprocess.check_call(f"cd tmp_git_clone && git archive --format=zip --output ../{zipfilename} HEAD",
                          shell=True)
    
    with open(filename, 'tw') as f:
        f.write(f"== git-repo-url: {repo_url}\n")
        f.write(f"== repo-name: {repo_name}\n")
        f.write(f"== snap-shot-of-repo: {zipfilename}\n")
        f.write(f"== cloned-on-date: {time.asctime()}\n")
        f.write(f"== copied-files-from-path-in-repo: {copy_from}\n")
        f.write("last commit:\n")
        output_bytes = subprocess.check_output(f"cd tmp_git_clone && git log HEAD^..HEAD", shell=True)
        f.write(f"{output_bytes.decode('utf8')}\n")
        f.write("== src file-list:\n")
        output_bytes = subprocess.check_output(f"cd tmp_git_clone && ls -l {copy_from}", shell=True)
        f.write(f"{output_bytes.decode('utf8')}\n")
        f.write(f"== see contents of {zipfilename} for additional info, examples, etc.\n")

    # if directory exists already, delete it so we start from a clean state
    if os.path.exists(f"oommf/app/oxs/local/{repo_name}"):
        subprocess.check_call(f"rm -rf -v oommf/app/oxs/local/{repo_name}", shell=True)

    # create target directory
    subprocess.check_call(f"mkdir -p -v oommf/app/oxs/local/{repo_name}", shell=True)

    src_path = os.path.relpath(os.path.join('tmp_git_clone', copy_from)) 
    copy_command = f"rsync -v {src_path}/* oommf/app/oxs/local/{repo_name}"
    print(f"Copy src files from {repo_name}:")
    print(f"    {copy_command}")
    subprocess.check_call(copy_command, shell=True)

    # copy additional information to target directory
    print(f"Provenance information recorded in {filename}")
    subprocess.check_call(f"mv -v {filename} oommf/app/oxs/local/{repo_name}", shell=True)
    subprocess.check_call(f"mv -v {zipfilename} oommf/app/oxs/local/{repo_name}", shell=True)

    # remove temporary checkout directory
    subprocess.check_call("rm -rf tmp_git_clone", shell=True)


if __name__ == "__main__":
    if len(sys.argv) != 3:
        print("python3 clone-log-and-extract-src.py URL FROMPATH\n")
        print("- Provide URL of repo from which we should git-clone.")
        print("- Provide the path from which to copy (FROMPATH).\n")
        print("Examples:\n")
        print("   python3 clone-log-and-extract-src.py https://github.com/joommf/oommf-extension-dmi-cnv.git src")
        print("   python3 clone-log-and-extract-src.py https://github.com/yuyahagi/oommf-mel.git .\n")

        print("More context:\n")
        print(doc)
        print()
        print(f"I received {len(sys.argv)} arguments ({sys.argv}), but need 3.")
        print()
    else:
        main(sys.argv)
