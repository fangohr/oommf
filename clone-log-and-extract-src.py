import os
import subprocess
import sys
import time


def main(argv):
    repo_url = argv[1]
    assert repo_url.endswith('.git')
    repo_name = os.path.split(repo_url)[1].split('.git')[0]
    print(f"Processing {repo_name}")
    subprocess.check_call(f"git clone {repo_url} tmp_git_clone", shell=True)

    # log what repository and version these files are from
    filename = f'extension-provenance-{repo_name}.log'
    with open(filename, 'tw') as f:
        f.write(f"git-repo-url: {repo_url}\n")
        f.write(f"repo-name: {repo_name}\n")
        f.write(f"cloned-on-date: {time.asctime()}\n")
        f.write("last commit:\n")
        output_bytes = subprocess.check_output(f"cd tmp_git_clone && git log HEAD^..HEAD", shell=True)
        f.write(f"{output_bytes.decode('utf8')}\n")
        f.write("file-list:\n")
        output_bytes = subprocess.check_output(f"cd tmp_git_clone && ls -l src/*", shell=True)
        f.write(f"{output_bytes.decode('utf8')}\n")
    subprocess.check_call(f"cp -v {filename} oommf/app/oxs/local/", shell=True)
    subprocess.check_call("cp -v tmp_git_clone/src/* oommf/app/oxs/local/", shell=True)
    subprocess.check_call("rm -rf tmp_git_clone", shell=True)



if __name__ == "__main__":
    assert len(sys.argv) >= 2
    print(f"sys.argv = {sys.argv}")
    #
    testargv = "https://github.com/joommf/oommf-extension-dmi-t.git"
    main([sys.argv[0]] + [testargv])
