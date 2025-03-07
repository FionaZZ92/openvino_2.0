# Converting a TensorFlow Model {#openvino_docs_MO_DG_prepare_model_convert_model_Convert_Model_From_TensorFlow}

@sphinxdirective

This page provides general instructions on how to run model conversion from a TensorFlow format to the OpenVINO IR format. The instructions are different depending on whether your model was created with TensorFlow v1.X or TensorFlow v2.X.

.. note:: TensorFlow models are supported via :doc:`FrontEnd API <openvino_docs_MO_DG_TensorFlow_Frontend>`. You may skip conversion to IR and read models directly by OpenVINO runtime API. Refer to the :doc:`inference example <openvino_docs_OV_UG_Integrate_OV_with_your_application>` for more details. Using ``convert_model`` is still necessary in more complex cases, such as new custom inputs/outputs in model pruning, adding pre-processing, or using Python conversion extensions.

To use model conversion API, install OpenVINO Development Tools by following the :doc:`installation instructions <openvino_docs_install_guides_install_dev_tools>`.

Converting TensorFlow 1 Models
###############################

Converting Frozen Model Format
+++++++++++++++++++++++++++++++

To convert a TensorFlow model, use the ``*mo*`` script to simply convert a model with a path to the input model ``*.pb*`` file:

.. code-block:: sh

   mo --input_model <INPUT_MODEL>.pb


Converting Non-Frozen Model Formats
+++++++++++++++++++++++++++++++++++

There are three ways to store non-frozen TensorFlow models and convert them by model conversion API:

1. **Checkpoint**. In this case, a model consists of two files: ``inference_graph.pb`` (or ``inference_graph.pbtxt``) and ``checkpoint_file.ckpt``.
If you do not have an inference graph file, refer to the `Freezing Custom Models in Python <#Freezing-Custom-Models-in-Python>`__  section.
To convert the model with the inference graph in ``.pb`` format, run the `mo` script with a path to the checkpoint file:

.. code-block:: sh

   mo --input_model <INFERENCE_GRAPH>.pb --input_checkpoint <INPUT_CHECKPOINT>

To convert the model with the inference graph in ``.pbtxt`` format, run the ``mo`` script with a path to the checkpoint file:

.. code-block:: sh

   mo --input_model <INFERENCE_GRAPH>.pbtxt --input_checkpoint <INPUT_CHECKPOINT> --input_model_is_text


2. **MetaGraph**. In this case, a model consists of three or four files stored in the same directory: ``model_name.meta``, ``model_name.index``,
``model_name.data-00000-of-00001`` (the numbers may vary), and ``checkpoint`` (optional).
To convert such TensorFlow model, run the `mo` script with a path to the MetaGraph ``.meta`` file:

.. code-block:: sh

   mo --input_meta_graph <INPUT_META_GRAPH>.meta


3. **SavedModel format**. In this case, a model consists of a special directory with a ``.pb`` file
and several subfolders: ``variables``, ``assets``, and ``assets.extra``. For more information about the SavedModel directory, refer to the `README <https://github.com/tensorflow/tensorflow/tree/master/tensorflow/python/saved_model#components>`__ file in the TensorFlow repository.
To convert such TensorFlow model, run the ``mo`` script with a path to the SavedModel directory:

.. code-block:: sh

   mo --saved_model_dir <SAVED_MODEL_DIRECTORY>


You can convert TensorFlow 1.x SavedModel format in the environment that has a 1.x or 2.x version of TensorFlow. However, TensorFlow 2.x SavedModel format strictly requires the 2.x version of TensorFlow.
If a model contains operations currently unsupported by OpenVINO, prune these operations by explicit specification of input nodes using the ``--input`` option.
To determine custom input nodes, display a graph of the model in TensorBoard. To generate TensorBoard logs of the graph, use the ``--tensorboard_logs`` option.
TensorFlow 2.x SavedModel format has a specific graph due to eager execution. In case of pruning, find custom input nodes in the ``StatefulPartitionedCall/*`` subgraph of TensorFlow 2.x SavedModel format.

Freezing Custom Models in Python
++++++++++++++++++++++++++++++++

When a network is defined in Python code, you have to create an inference graph file. Graphs are usually built in a form
that allows model training. That means all trainable parameters are represented as variables in the graph.
To be able to use such graph with model conversion API, it should be frozen and dumped to a file with the following code:

.. code-block:: python

   import tensorflow as tf
   from tensorflow.python.framework import graph_io
   frozen = tf.compat.v1.graph_util.convert_variables_to_constants(sess, sess.graph_def, ["name_of_the_output_node"])
   graph_io.write_graph(frozen, './', 'inference_graph.pb', as_text=False)

Where:

* ``sess`` is the instance of the TensorFlow Session object where the network topology is defined.
* ``["name_of_the_output_node"]`` is the list of output node names in the graph; ``frozen`` graph will include only those nodes from the original ``sess.graph_def`` that are directly or indirectly used to compute given output nodes. The ``'name_of_the_output_node'`` is an example of a possible output node name. You should derive the names based on your own graph.
* ``./`` is the directory where the inference graph file should be generated.
* ``inference_graph.pb`` is the name of the generated inference graph file.
* ``as_text`` specifies whether the generated file should be in human readable text format or binary.

Converting TensorFlow 2 Models
###############################

To convert TensorFlow 2 models, ensure that `openvino-dev[tensorflow2]` is installed via `pip`.
TensorFlow 2.X officially supports two model formats: SavedModel and Keras H5 (or HDF5).
Below are the instructions on how to convert each of them.

SavedModel Format
+++++++++++++++++

A model in the SavedModel format consists of a directory with a ``saved_model.pb`` file and two subfolders: ``variables`` and ``assets``.
To convert such a model, run the `mo` script with a path to the SavedModel directory:

.. code-block:: sh

   mo --saved_model_dir <SAVED_MODEL_DIRECTORY>

TensorFlow 2 SavedModel format strictly requires the 2.x version of TensorFlow installed in the
environment for conversion to the Intermediate Representation (IR).

If a model contains operations currently unsupported by OpenVINO™,
prune these operations by explicit specification of input nodes using the ``--input`` or ``--output``
options. To determine custom input nodes, visualize a model graph in the TensorBoard.

TensorFlow 2 SavedModel format has a specific graph structure due to eager execution. In case of
pruning, find custom input nodes in the ``StatefulPartitionedCall/*`` subgraph.

Since the 2023.0 release, direct pruning of models in SavedModel format is not supported.
It is essential to freeze the model before pruning. Use the following code snippet for model freezing: 

.. code-block:: python

   import tensorflow as tf
   from tensorflow.python.framework.convert_to_constants import convert_variables_to_constants_v2
   saved_model_dir = "./saved_model"
   imported = tf.saved_model.load(saved_model_dir)
   # retrieve the concrete function and freeze
   concrete_func = imported.signatures[tf.saved_model.DEFAULT_SERVING_SIGNATURE_DEF_KEY]
   frozen_func = convert_variables_to_constants_v2(concrete_func,
                                                   lower_control_flow=False,
                                                   aggressive_inlining=True)
   # retrieve GraphDef and save it into .pb format
   graph_def = frozen_func.graph.as_graph_def(add_shapes=True)
   tf.io.write_graph(graph_def, '.', 'model.pb', as_text=False)

Keras H5
++++++++

If you have a model in the HDF5 format, load the model using TensorFlow 2 and serialize it in the
SavedModel format. Here is an example of how to do it:

.. code-block:: python

   import tensorflow as tf
   model = tf.keras.models.load_model('model.h5')
   tf.saved_model.save(model,'model')


The Keras H5 model with a custom layer has specifics to be converted into SavedModel format.
For example, the model with a custom layer ``CustomLayer`` from ``custom_layer.py`` is converted as follows:

.. code-block:: python

   import tensorflow as tf
   from custom_layer import CustomLayer
   model = tf.keras.models.load_model('model.h5', custom_objects={'CustomLayer': CustomLayer})
   tf.saved_model.save(model,'model')


Then follow the above instructions for the SavedModel format.

.. note::

   Do not use other hacks to resave TensorFlow 2 models into TensorFlow 1 formats.

Command-Line Interface (CLI) Examples Using TensorFlow-Specific Parameters
##########################################################################

* Launching model conversion for Inception V1 frozen model when model file is a plain text protobuf:

  .. code-block:: sh

     mo --input_model inception_v1.pbtxt --input_model_is_text -b 1


* Launching model conversion for Inception V1 frozen model and dump information about the graph to TensorBoard log dir ``/tmp/log_dir``

  .. code-block:: sh

     mo --input_model inception_v1.pb -b 1 --tensorboard_logdir /tmp/log_dir


* Launching model conversion for BERT model in the SavedModel format, with three inputs. Specify explicitly the input shapes where the batch size and the sequence length equal 2 and 30 respectively.

  .. code-block:: sh

     mo --saved_model_dir BERT --input mask,word_ids,type_ids --input_shape [2,30],[2,30],[2,30]

Conversion of TensorFlow models from memory using Python API
############################################################

Model conversion API supports passing TensorFlow/TensorFlow2 models directly from memory.

* ``tf.keras.Model``

  .. code-block:: python

     model = tf.keras.applications.ResNet50(weights="imagenet")
     ov_model = convert_model(model)


* ``tf.keras.layers.Layer``. Requires setting the "input_shape".

  .. code-block:: python

     import tensorflow_hub as hub

     model = hub.KerasLayer("https://tfhub.dev/google/imagenet/mobilenet_v1_100_224/classification/5")
     ov_model = convert_model(model, input_shape=[-1, 224, 224, 3])

* ``tf.Module``. Requires setting the "input_shape".

  .. code-block:: python

     class MyModule(tf.Module):
        def __init__(self, name=None):
           super().__init__(name=name)
           self.variable1 = tf.Variable(5.0, name="var1")
           self.variable2 = tf.Variable(1.0, name="var2")
        def __call__(self, x):
           return self.variable1 * x + self.variable2

     model = MyModule(name="simple_module")
     ov_model = convert_model(model, input_shape=[-1])

* ``tf.compat.v1.Graph``

  .. code-block:: python

     with tf.compat.v1.Session() as sess:
        inp1 = tf.compat.v1.placeholder(tf.float32, [100], 'Input1')
        inp2 = tf.compat.v1.placeholder(tf.float32, [100], 'Input2')
        output = tf.nn.relu(inp1 + inp2, name='Relu')
        tf.compat.v1.global_variables_initializer()
        model = sess.graph

     ov_model = convert_model(model)

* ``tf.compat.v1.GraphDef``

  .. code-block:: python

     with tf.compat.v1.Session() as sess:
        inp1 = tf.compat.v1.placeholder(tf.float32, [100], 'Input1')
        inp2 = tf.compat.v1.placeholder(tf.float32, [100], 'Input2')
        output = tf.nn.relu(inp1 + inp2, name='Relu')
        tf.compat.v1.global_variables_initializer()
        model = sess.graph_def

     ov_model = convert_model(model)

* ``tf.function``

  .. code-block:: python

     @tf.function(
        input_signature=[tf.TensorSpec(shape=[1, 2, 3], dtype=tf.float32),
                         tf.TensorSpec(shape=[1, 2, 3], dtype=tf.float32)])
     def func(x, y):
        return tf.nn.sigmoid(tf.nn.relu(x + y))

     ov_model = convert_model(func)

* ``tf.compat.v1.session``

  .. code-block:: python

     with tf.compat.v1.Session() as sess:
        inp1 = tf.compat.v1.placeholder(tf.float32, [100], 'Input1')
        inp2 = tf.compat.v1.placeholder(tf.float32, [100], 'Input2')
        output = tf.nn.relu(inp1 + inp2, name='Relu')
        tf.compat.v1.global_variables_initializer()

        ov_model = convert_model(sess)

* ``tf.train.checkpoint``

  .. code-block:: python

     model = tf.keras.Model(...)
     checkpoint = tf.train.Checkpoint(model)
     save_path = checkpoint.save(save_directory)
     # ...
     checkpoint.restore(save_path)
     ov_model = convert_model(checkpoint)

Supported TensorFlow and TensorFlow 2 Keras Layers
##################################################

For the list of supported standard layers, refer to the :doc:`Supported Operations <openvino_resources_supported_operations_frontend>` page.

Frequently Asked Questions (FAQ)
################################

The model conversion API provides explanatory messages if it is unable to run to completion due to typographical errors, incorrectly used options, or other issues. The message describes the potential cause of the problem and gives a link to the :doc:`Model Optimizer FAQ <openvino_docs_MO_DG_prepare_model_Model_Optimizer_FAQ>`. The FAQ provides instructions on how to resolve most issues. The FAQ also includes links to relevant sections in :doc:`Convert a Model <openvino_docs_MO_DG_Deep_Learning_Model_Optimizer_DevGuide>` to help you understand what went wrong.

Summary
#######

In this document, you learned:

* Basic information about how the model conversion API works with TensorFlow models.
* Which TensorFlow models are supported.
* How to freeze a TensorFlow model.
* How to convert a trained TensorFlow model using model conversion API with both framework-agnostic and TensorFlow-specific command-line parameters.

Additional Resources
####################

See the :doc:`Model Conversion Tutorials <openvino_docs_MO_DG_prepare_model_convert_model_tutorials>` page for a set of tutorials providing step-by-step instructions for converting specific TensorFlow models. Here are some examples:

* :doc:`Convert TensorFlow EfficientDet Models <openvino_docs_MO_DG_prepare_model_convert_model_tf_specific_Convert_EfficientDet_Models>`
* :doc:`Convert TensorFlow FaceNet Models <openvino_docs_MO_DG_prepare_model_convert_model_tf_specific_Convert_FaceNet_From_Tensorflow>`
* :doc:`Convert TensorFlow Object Detection API Models <openvino_docs_MO_DG_prepare_model_convert_model_tf_specific_Convert_Object_Detection_API_Models>`

@endsphinxdirective
