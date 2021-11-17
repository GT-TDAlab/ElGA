FROM centos:8.4.2105

# Install necessary packages
RUN dnf update -y && dnf upgrade -y && \
        dnf group install 'Development Tools' -y && \
        dnf install -y cmake psmisc numactl vim tmux git-lfs \
                ncurses-devel readline-devel openssh-server \
                sudo glibc-langpack-en

# Install pdsh
USER root
WORKDIR /root
RUN git clone https://github.com/chaos/pdsh.git --depth 1 --branch pdsh-2.34 pdsh-2.34 && \
    mkdir -p rpmbuild/SOURCES && \
    pushd pdsh-2.34 && echo H4sIAIOiumACA61Ta0/bMBT9PP+KKwYIFBxKXxQkNELboYi0YX1omoQUObbTeiRx5jgaWtX/Picpo+0mbR9mKbHvybn29Tk3TEQRYLwQGsj5iDzzSMTcJgmE2xESKeMv0Gl2Wa/bse1GqxW2Oj24aDS67TbCGO9mI8uy9na4vQXcO7sEq3yZQKQ0LhiHwxMtsyBXlAl1ek5lGolFlYgNw06eESBw+p7fd7zAGX30nPsp3MDOwC7UeYY5n/kj52EY+I8z1x/vUyOpuFikwESucfhDZE2Ep/O7gTv5bdPXYSoDzXOdA5MUcqpEZtab86x/SP5DmrlSQkSqzcMVpjEnKY4lJfE1gnf4hLJdVeD4GFQCWEVACi2TtuY2JXTJTxHbti9j+dLOM06N9L/WG+to1Lnilxe2TbpXPX5Fdq17Y9fGvcWlbc1m56wLVjlV1kVKJkBSIHFZtOYMZlJ9Kzh8laGN4P1/HaVYR2EhYoYs+zyUUudakcyAtZiF4kaATMmFIomZTcO93BytPgQbKKih66PVHrJewxOqXCrZPCVhzAPGw2Kx3ri39fm70MtA5cv1lrlPtTi9RtnVzd5FLc7GqIPDyeMouJu73iCY+P7swBRc/gq5mRmPiNbqBJ8pc53qdVrCplH6/uMXd3wPk6EzGA1hPPw8hYE77XuOOxpObM8b78TzPsJVXs23HyZ37dd1IllhznsNP+VjrpH1F7YpYxWEpmWEWlddsAfQbAdQFfITsY4aHkQEAAA= | base64 -d | gunzip | patch -p1 && popd && \
    tar czvf rpmbuild/SOURCES/pdsh-2.34-1.tar.gz pdsh-2.34 && \
    rpmbuild -bb pdsh-2.34/pdsh.spec && \
    dnf install rpmbuild/RPMS/x86_64/pdsh-2.34-1.x86_64.rpm rpmbuild/RPMS/x86_64/pdsh-rcmd-ssh-2.34-1.x86_64.rpm -y && \
    echo "user ALL=NOPASSWD:ALL" >> /etc/sudoers

# Install ZeroMQ
RUN git clone https://github.com/zeromq/libzmq.git --depth 1 --branch v4.3.4 && \
    mkdir libzmq-build && \
    pushd libzmq-build && \
    cmake -DCMAKE_BUILD_TYPE=Release ../libzmq && \
    make -j`grep -c ^processor /proc/cpuinfo` && \
    make install && \
    popd

# Create a user
ARG UID=1000
ARG GID=1000
RUN groupadd -g $GID user && useradd -m -s /bin/bash -N -g user -u $UID user && \
    chmod 0755 /home/user

# Move to running as the user
USER user
WORKDIR /home/user

# Add in an example graph
RUN mkdir graphs && pushd graphs && \
    curl -LO https://snap.stanford.edu/data/email-EuAll.txt.gz && \
    gunzip email-EuAll.txt && popd

# Prepare for SSH setup
RUN mkdir .ssh && chmod 0700 .ssh && \
    cat /dev/zero | ssh-keygen -q -N "" && \
    cp .ssh/id_rsa.pub .ssh/authorized_keys && \
    chmod 0600 .ssh/authorized_keys

# Setup files to describe the cluster
RUN sudo mkdir -p /scratch /cscratch && \
    sudo chmod 1777 /scratch /cscratch && \
    mkdir /scratch/elga && \
    echo "localhost" > /cscratch/nodes && \
    echo "localhost" > /cscratch/nodes-all && \
    echo "localhost" > /cscratch/nodes-full && \
    echo "127.0.0.1" | sudo tee /ip

# Setup the entrypoint to configure SSH and start tmux
RUN echo H4sIAAAAAAAAA41Sy07DMBC85yu2D9FTsmkPcECq6AHRf0BAXWcTmzp28KPQC9+O4yA1oCJhH7zanR3Pjj2b4F5q3DMnskzW8AjT+WwKOb1BCU+34AXpLIO4iAsD03v1sAEfvLGSKZDaeaY5QTysl7opimI6wF2oDDgn8gOdGtKQb+BqjRUdUQelRhgMzqLrRUR0lfJBe6milJwAbdCojTKN1L2enlQRdbDsY01nHtv+AA83eBO4gE8sIjcetHnXL8I471KRi9ZUUF6X5V+Ib/2OszjAFpThTPVVWK8vtMBqNOCF9uXqpijjXv6jfVDfho+R+YstOxIw0DI6XrHTZJFlpBwNkC5E+wAdt8xzgaQalvLRl9zWsA9SDe7yDnIGKExL0XuyCYqpXuzZYYS8QIkjnpYdCObL31TD6ELWPkUJlb/CrrHx2XIOz501nJwzFrAPkXdB6trsoP9cqafA9M/md4MM01XnIKtl9gWtA1wXtgIAAA== | \
    base64 -d | gunzip | sudo tee /entry && \
    sudo chmod +x /entry

# Setup tmux to be a little bit nicer
RUN echo H4sIAAAAAAAAA22PzQrCMBCE7z7FQgk9BbwJUguCohe96EVCDmmzTUPXBvKDr6+tYKu4p+Gb3WE2YARuIEQVU+CVgYpU3S3CF24GnPCHemvaCHkmGrMxHrGXZZkJjY1KFGUmKkda7umwhYvtDSGcnUbYubpDD9cUnbeK5gefnKKYMIwzM8Wohz6S3Ti7c6bf9qO18cWOa3aarcspKv/3ACfsYbVcPAFlMCuRCQEAAA== | \
    base64 -d | gunzip | tee .tmux.conf

# Copy ElGA
COPY implementation elga
RUN sudo chown -R user elga

# Perform a shallow clone of Abseil
RUN rm -rf /home/user/elga/third-party/abseil-cpp && git clone https://github.com/abseil/abseil-cpp /home/user/elga/third-party/abseil-cpp --depth 1 --branch 20210324.2

# Compile ElGA
RUN mkdir /scratch/elga/build && pushd /scratch/elga/build && cmake -DARCH_NATIVE=OFF /home/user/elga && \
    make -j`grep -c ^processor /proc/cpuinfo` && \
    make test && \
    chmod o+rwX -R . && \
    chmod g+rwX -R . && \
    popd && \
    pushd /home/user/elga && \
    rm -r build && ln -s /scratch/elga/build build && \
    cp -a /scratch/elga/build build.bak && \
    popd

# Copy in the scripts
COPY scripts scripts
RUN sudo chown -R user /home/user/scripts

# Modify the scripts to suit the single machine cluster
WORKDIR /home/user/scripts
RUN sed 's/PRE=.*$/PRE=""/' vars.sh | sed 's/public.*tion\///' | sed 's/-4/-1/g' | sed 's/PN:-8/PN:-4/' | sed 's/mask=16/mask=8/' > n && mv n vars.sh && \
    sed -i 's/numactl.*--//' start-elga.sh && sed -i 's/(nid/(2+nid/' start-elga.sh

ENTRYPOINT ["/entry"]
