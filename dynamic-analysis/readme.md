# Dynamic Analysis

Stable-release distributions avoid using the upstream branch. So the glibc on your GNU/Linux machine may or may not be v2.43.

To ensure reproducibility, a Dockerfile is provided in the root directory that does the whole setup. It builds glibc-2.43 from scratch. The details of the setup are discussed below.

If you have cloned the repository and reading locally, open terminal in the repo's root directory and run the docker commands.

If you are reading on GitHub, you can either clone the repository and follow along, or just `wget` the `Dockerfile`. There is absolutely no compulsion to clone the repo.

Below are the commands.

1. Get the Dockerfile.
   ```bash
   wget https://raw.githubusercontent.com/aggrawal-ankur/systems-dives/refs/heads/main/glibc-malloc/dynamic-analysis-code/setup
   ```

2. Build the docker image.
   ```bash
   # docker build -t <image-name>
   docker build -t glibc-malloc-exp-img
   ```

3. Create and run a container.
   ```bash
   # docker run -it --name <container-name> <image-name>
   docker run -it --name glibc-exp-cont  glibc-malloc-exp-img
   ```

Later on, use these commands to reuse the container.

1. Start the container.
   ```bash
   docker start <container-name>
   # or
   docker start <container-id>
   ```

2. Attach to the container.
   ```bash
   docker attach <container-name>
   # or
   docker attach <container-id>
   ```

3. [CTRL + D] to exit the container.

**Notes**:
  1. If you don't have docker enabled on OS start, you have to use a utility like systemctl to enable docker before using it.
  2. If your user is not in the `docker` group, append `sudo` before each command, except `wget`.
  3. The setup builds glibc-2.43 from source, so it is better to reuse the container instead of spinning a new one every time.

That's the setup I use. If you are experienced with docker, use the Dockerfile however you want.
