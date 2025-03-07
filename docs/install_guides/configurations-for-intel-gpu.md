# Configurations for Intel® Processor Graphics (GPU) with OpenVINO™ {#openvino_docs_install_guides_configurations_for_intel_gpu}

@sphinxdirective

.. _gpu guide:

To use the OpenVINO™ GPU plug-in and transfer the inference to the graphics of the Intel® processor (GPU), the Intel® graphics driver must be properly configured on the system.

Linux
#####

To use a GPU device for OpenVINO inference, you must meet the following prerequisites:

- Use a supported Linux kernel as per the `documentation <https://dgpu-docs.intel.com/driver/kernel-driver-types.html>`__
- Install ``intel-i915-dkms`` and ``xpu-smi`` kernel modules as described in the `installation documentation <https://dgpu-docs.intel.com/driver/installation.html>`__
- Install GPU Runtime packages:

  - `The Intel(R) Graphics Compute Runtime for oneAPI Level Zero and OpenCL(TM) Driver <https://github.com/intel/compute-runtime/releases/latest>`__
  - `Intel Graphics Memory Management Library <https://github.com/intel/gmmlib>`__
  - `Intel® Graphics Compiler for OpenCL™ <https://github.com/intel/intel-graphics-compiler>`__
  - `OpenCL ICD loader package <https://github.com/KhronosGroup/OpenCL-ICD-Loader>`__

Depending on your operating system, there may be different methods to install the above packages. Below are the instructions on how to install the packages on supported Linux distributions.

.. tab-set::

   .. tab-item:: Ubuntu 22.04 LTS
      :sync: ubuntu22

      Download and install the `deb` packages published `here <https://github.com/intel/compute-runtime/releases/latest>`__ and install the apt package `ocl-icd-libopencl1` with the OpenCl ICD loader.
      
      Alternatively, you can add the apt repository by following the `installation guide <https://dgpu-docs.intel.com/driver/installation.html#ubuntu-install-steps>`__. Then install the `ocl-icd-libopencl1`, `intel-opencl-icd`, `intel-level-zero-gpu` and `level-zero` apt packages:
      
      .. code-block:: sh
      
         apt-get install -y ocl-icd-libopencl1 intel-opencl-icd intel-level-zero-gpu level-zero

   .. tab-item:: Ubuntu 20.04 LTS
      :sync: ubuntu20

      Ubuntu 20.04 LTS is not updated with the latest driver versions. You can install the updated versions up to the version 22.43 from apt:
      
      .. code-block:: sh
         
         apt-get update && apt-get install -y --no-install-recommends curl gpg gpg-agent && \
         curl https://repositories.intel.com/graphics/intel-graphics.key | gpg --dearmor --output /usr/share/keyrings/intel-graphics.gpg && \
         echo 'deb [arch=amd64 signed-by=/usr/share/keyrings/intel-graphics.gpg] https://repositories.intel.com/graphics/ubuntu focal-legacy main' | tee  /etc/apt/sources.list.d/intel.gpu.focal.list && \
         apt-get update
         apt-get update && apt-get install -y --no-install-recommends intel-opencl-icd intel-level-zero-gpu level-zero
      
      Alternatively, download older `deb` version from `here <https://github.com/intel/compute-runtime/releases>`__. Note that older driver version might not include some of the bug fixes and might be not supported on some latest platforms. Check the supported hardware for the versions you are installing.

   .. tab-item:: RedHat UBI 8
      :sync: redhat8

      Follow the `guide <https://dgpu-docs.intel.com/driver/installation.html#rhel-install-steps>`__ to add Yum repository.
      
      Install following packages: 
      
      .. code-block:: sh
      
         yum install intel-opencl level-zero intel-level-zero-gpu intel-igc-core intel-igc-cm intel-gmmlib intel-ocloc
      
      Install the OpenCL ICD Loader via:
      
      .. code-block:: sh
      
         rpm -ivh http://mirror.centos.org/centos/8-stream/AppStream/x86_64/os/Packages/ocl-icd-2.2.12-1.el8.x86_64.rpm
      
.. _gpu guide windows:

Windows
#######

To install the Intel Graphics Driver for Windows, follow the `driver installation instructions <https://www.intel.com/content/www/us/en/support/articles/000005629/graphics.html>`_.

To check if the driver has been installed:

1. Type **device manager** in the **Search Windows** field and press Enter. **Device Manager** will open.
2. Click the drop-down arrow to display **Display Adapters**. You can see the adapter that is installed in your computer: 

   .. image:: _static/images/DeviceManager.PNG
      :width: 400

3. Right-click on the adapter name and select **Properties**.
4. Click the **Driver** tab to view the driver version.

   .. image:: _static/images/DeviceDriverVersion.svg
      :width: 400

Your device driver has been updated and is now ready to use your GPU.

.. _wsl-install:

Windows Subsystem for Linux (WSL)
#################################

WSL allows developers to run a GNU/Linux development environment for the Windows operating system. Using the GPU in WSL is very similar to a native Linux environment.

.. note::

   Make sure your Intel graphics driver is updated to version **30.0.100.9955** or later. You can download and install the latest GPU host driver `here <https://www.intel.com/content/www/us/en/download/19344/intel-graphics-windows-dch-drivers.html>`__.

Below are the required steps to make it work with OpenVINO:

- Install the GPU drivers as described :ref:`above <wsl-instal>`.
- Run the following commands in PowerShell to view the latest version of WSL2:

  .. code-block:: sh

     wsl --update
     wsl --shutdown
  
- When booting Ubuntu 20.04 or Ubuntu 22.04, install the same drivers as described above in the Linux section

.. note:: 
   
   In WSL, the GPU device is accessed via the character device `/dev/drx`, while for native Linux OS it is accessed via `/dev/dri`.

Additional Resources
####################

The following Intel® Graphics Driver versions were used during OpenVINO's internal validation:

+------------------+-------------------------------------------------------------------------------------------+
| Operation System | Driver version                                                                            |
+==================+===========================================================================================+
| Ubuntu 22.04     | `22.43.24595.30 <https://github.com/intel/compute-runtime/releases/tag/22.43.24595.30>`__ |
+------------------+-------------------------------------------------------------------------------------------+
| Ubuntu 20.04     | `22.35.24055 <https://github.com/intel/compute-runtime/releases/tag/22.35.24055>`__       |
+------------------+-------------------------------------------------------------------------------------------+
| Ubuntu 18.04     | `21.38.21026 <https://github.com/intel/compute-runtime/releases/tag/21.38.21026>`__       |
+------------------+-------------------------------------------------------------------------------------------+
| CentOS 7         | `19.41.14441 <https://github.com/intel/compute-runtime/releases/tag/19.41.14441>`__       |
+------------------+-------------------------------------------------------------------------------------------+
| RHEL 8           | `22.28.23726 <https://github.com/intel/compute-runtime/releases/tag/22.28.23726>`__       |
+------------------+-------------------------------------------------------------------------------------------+


What’s Next?
############

* :doc:`GPU Device <openvino_docs_OV_UG_supported_plugins_GPU>`
* :doc:`Install Intel® Distribution of OpenVINO™ toolkit for Linux from a Docker Image <openvino_docs_install_guides_installing_openvino_docker_linux>`
* `Docker CI framework for Intel® Distribution of OpenVINO™ toolkit <https://github.com/openvinotoolkit/docker_ci/blob/master/README.md>`__
* `Get Started with DockerHub CI for Intel® Distribution of OpenVINO™ toolkit <https://github.com/openvinotoolkit/docker_ci/blob/master/get-started.md>`__
* `Dockerfiles with Intel® Distribution of OpenVINO™ toolkit <https://github.com/openvinotoolkit/docker_ci/blob/master/dockerfiles/README.md>`__

@endsphinxdirective


