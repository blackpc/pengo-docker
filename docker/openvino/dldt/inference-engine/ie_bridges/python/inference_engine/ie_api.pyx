# Copyright (C) 2018 Intel Corporation
#
# SPDX-License-Identifier: Apache-2.0
#

#distutils: language=c++
from cython.operator cimport dereference as deref
from .cimport ie_api_impl_defs as C
from .ie_api_impl_defs cimport Blob, TensorDesc, SizeVector, Precision
from libcpp.string cimport string
from libcpp.vector cimport vector
from libcpp.map cimport map
from libcpp.memory cimport unique_ptr
from libc.stdint cimport int64_t
import os
import numpy as np
from copy import deepcopy

cdef extern from "<utility>" namespace "std" nogil:
    cdef unique_ptr[C.IEExecNetwork] move(unique_ptr[C.IEExecNetwork])

cdef string to_std_string(str py_string):
    return py_string.encode()

cdef to_py_string(const string & std_string):
    return bytes(std_string).decode()

cdef dict_to_c_map(py_dict):
    cdef map[string, string] c_map
    for k, v in py_dict.items():
        if type(k) != str or type(v) != str:
            raise TypeError("Only string keys and values are allowed!")
        c_map[k.encode()] = v.encode()
    return c_map

supported_precisions = ["FP32", "FP16", "Q78", "I32", "I16", "I8", "U32", "U16"]
supported_layouts = ["NCHW", "NHWC", "OIHW", "C", "CHW", "HW", "NC", "CN", "BLOCKED"]
known_plugins = ['CPU', 'GPU', 'FPGA', 'MYRIAD', 'HETERO']

def get_version():
    return C.get_version().decode()

cdef class IENetLayer:
    @property
    def name(self):
        return self.impl.name.decode()
    @property
    def type(self):
        return self.impl.type.decode()
    @property
    def precision(self):
        return self.impl.precision.decode()
    @property
    def affinity(self):
        return self.impl.affinity.decode()
    @property
    def weights(self):
        cdef map[string, Blob.Ptr] c_weights_map
        c_weights_map = self.impl.getWeights()
        weights_map = {}
        cdef BlobBuffer weights_buffer
        for weights in c_weights_map:
            weights_buffer = BlobBuffer()
            weights_buffer.reset(weights.second)
            weights_map[weights.first.decode()] = weights_buffer.to_numpy()
        return weights_map

    @property
    def params(self):
        return {k.decode(): v.decode() for k, v in self.impl.params}

    @affinity.setter
    def affinity(self, target_affinity):
        self.impl.setAffinity(target_affinity.encode())
    @params.setter
    def params(self, params_map):
        self.impl.setParams(dict_to_c_map(params_map))

    @precision.setter
    def precision(self, precision: str):
        self.impl.setPrecision(precision.upper().encode())


cdef class InputInfo:
    @property
    def precision(self):
        return self.impl.precision.decode()
    @property
    def layout(self):
        return self.impl.layout.decode()
    @property
    def shape(self):
        return self.impl.dims

    @precision.setter
    def precision(self, precision):
        if precision.upper() not in supported_precisions:
            raise AttributeError(
                "Unsupported precision {}! List of supported precisions: {}".format(precision, supported_precisions))
        self.impl.setPrecision(precision.encode())
    @layout.setter
    def layout(self, layout):
        if layout.upper() not in supported_layouts:
            raise AttributeError(
                "Unsupported layout {}! List of supported layouts: {}".format(layout, supported_layouts))
        self.impl.setLayout(layout.encode())


cdef class OutputInfo:
    @property
    def precision(self):
        return self.impl.precision.decode()
    @property
    def layout(self):
        return self.impl.layout.decode()
    @property
    def shape(self):
        return self.impl.dims
    @precision.setter
    def precision(self, precision):
        if precision.upper() not in supported_precisions:
            raise AttributeError(
                "Unsupported precision {}! List of supported precisions: {}".format(precision, supported_precisions))
        self.impl.setPrecision(precision.encode())
    # @layout.setter
    # def layout(self, layout):
    #     self.impl.setLayout(layout.encode())

cdef class ExecutableNetwork:
    def __init__(self):
        self._requests = []

    def infer(self, inputs=None):
        current_request = self.requests[0]
        current_request.infer(inputs)
        return deepcopy(current_request.outputs)


    def start_async(self, request_id, inputs=None):
        if request_id not in list(range(len(self.requests))):
            raise ValueError("Incorrect request_id specified!")
        current_request = self.requests[request_id]
        current_request.async_infer(inputs)
        return current_request

    @property
    def requests(self):
        return self._requests

cdef class InferRequest:
    def __init__(self):
        self._inputs = {}
        self._outputs = {}

    cpdef BlobBuffer _get_input_buffer(self, const string & blob_name):
        cdef BlobBuffer buffer = BlobBuffer()
        buffer.reset(deref(self.impl).getInputBlob(blob_name))
        return buffer

    cpdef BlobBuffer _get_output_buffer(self, const string & blob_name):
        cdef BlobBuffer buffer = BlobBuffer()
        buffer.reset(deref(self.impl).getOutputBlob(blob_name))
        return buffer

    cpdef infer(self, inputs=None):
        if inputs is not None:
            self._fill_inputs(inputs)

        deref(self.impl).infer()

    cpdef async_infer(self, inputs=None):
        if inputs is not None:
            self._fill_inputs(inputs)

        deref(self.impl).infer_async()

    cpdef wait(self, timeout=None):
        if timeout is None:
            timeout = -1
        return deref(self.impl).wait(<int64_t> timeout)

    cpdef get_perf_counts(self):
        cdef map[string, C.ProfileInfo] c_profile = deref(self.impl).getPerformanceCounts()
        profile = {}
        for l in c_profile:
            info = l.second
            # TODO: add execution index. Check if unsigned int is properly converted to int in python.
            profile[l.first.decode()] = {"status": info.status.decode(), "exec_type": info.exec_type.decode(),
                                         "layer_type": info.layer_type.decode(), "real_time": info.real_time,
                                         "cpu_time": info.cpu_time}
        return profile

    @property
    def inputs(self):
        return self._inputs

    @property
    def outputs(self):
        return self._outputs

    def _fill_inputs(self, inputs):
        for k, v in inputs.items():
            self._inputs[k][:] = v

cdef class IENetwork:
    @property
    def name(self):
        name = bytes(self.impl.name)
        return name.decode()

    @property
    def inputs(self):
        cdef map[string, C.InputInfo] c_inputs = self.impl.getInputs()
        inputs = {}
        cdef InputInfo in_info
        for input in  c_inputs:
            in_info = InputInfo()
            in_info.impl = input.second
            inputs[input.first.decode()] = in_info
        return inputs

    @property
    def outputs(self):
        cdef map[string, C.OutputInfo] c_outputs = self.impl.getOutputs()
        outputs = {}
        cdef OutputInfo out_info
        for out in  c_outputs:
            out_info = OutputInfo()
            out_info.impl = out.second
            outputs[out.first.decode()] = out_info
        return outputs

    @property
    def batch_size(self):
        return self.impl.batch_size

    @batch_size.setter
    def batch_size(self, batch: int):
        if batch <= 0:
            raise AttributeError("Invalid batch size {}! Batch size should be positive integer value".format(batch))
        self.impl.setBatch(batch)
        self.impl.batch_size = batch

    @property
    def layers(self):
        cdef map[string, C.IENetLayer] c_layers = <map[string, C.IENetLayer]> self.impl.getLayers()
        layers = {}
        cdef IENetLayer net_l = IENetLayer()
        for l in c_layers:
            net_l = IENetLayer()
            net_l.impl = l.second
            layers[l.first.decode()] = net_l
        return layers

    @classmethod
    def from_ir(cls, model: str, weights: str):
        if not os.path.isfile(model):
            raise Exception("Path to the model {} doesn't exists or it's a directory".format(model))
        if not os.path.isfile(weights):
            raise Exception("Path to the weights {} doesn't exists or it's a directory".format(weights))
        net_reader = IENetReader()
        return net_reader.read(model, weights)

    # TODO: Use enum with precision type instead of srting parameter when python2 support will not be required.
    def add_outputs(self, outputs, precision="FP32"):
        if precision.upper() not in supported_precisions:
            raise AttributeError(
                "Unsupported precision {}! List of supported precisions: {}".format(precision, supported_precisions))
        if not isinstance(outputs, list):
            outputs = [outputs]
        cdef vector[string] _outputs
        for l in outputs:
            _outputs.push_back(l.encode())
        self.impl.addOutputs(_outputs, precision.upper().encode())

    def reshape(self, input_shapes: dict):
        cdef map[string, vector[size_t]] c_input_shapes;
        cdef vector[size_t] c_shape
        net_inputs = self.inputs
        for input, shape in input_shapes.items():
            if input not in net_inputs:
                raise AttributeError("Specified {} layer not in network inputs {}! ".format(input, net_inputs))
            for v in shape:
                c_shape.push_back(v)
            c_input_shapes[input.encode()] = c_shape
        self.impl.reshape(c_input_shapes)

cdef class IEPlugin:
    def __cinit__(self, device: str, plugin_dirs=None):
        plugin_base = device.split(':')[0]
        if plugin_base not in known_plugins:
            raise ValueError("Unknown plugin: {}, expected one of: {}"
                             .format(plugin_base, ",".join(known_plugins)))
        if plugin_dirs is None:
            plugin_dirs = [""]
        elif isinstance(plugin_dirs, str):
            plugin_dirs = [plugin_dirs]

        # add package directory to plugin_dirs
        lib_location = os.path.dirname(os.path.realpath(__file__))
        plugin_dirs.append(lib_location)

        cpdef string device_ = <string> device.encode()
        cdef vector[string] dirs_
        for d in plugin_dirs:
            dirs_.push_back(<string> d.encode())

        self.impl = C.IEPlugin(device_, dirs_)

    cpdef ExecutableNetwork load(self, IENetwork network, int num_requests=1, config=None):
        if num_requests <= 0:
            raise ValueError(
                "Incorrect number of requests specified: {}. Expected positive integer number.".format(num_requests))
        cdef ExecutableNetwork exec_net = ExecutableNetwork()
        cdef vector[string] inputs_list
        cdef vector[string] outputs_list
        cdef map[string, string] c_config

        if config:
            for k, v in config.items():
                c_config[to_std_string(k)] = to_std_string(v)

        exec_net.impl = move(self.impl.load(network.impl, num_requests, c_config))

        requests = []
        for i in range(deref(exec_net.impl).infer_requests.size()):
            infer_request = InferRequest()
            infer_request.impl = &(deref(exec_net.impl).infer_requests[i])

            inputs_list = infer_request.impl.getInputsList()
            outputs_list = infer_request.impl.getOutputsList()

            for input_b in inputs_list:
                input_s = input_b.decode()
                infer_request._inputs[input_s] = infer_request._get_input_buffer(input_b).to_numpy()

            for output_b in outputs_list:
                output_s = output_b.decode()
                infer_request._outputs[output_s] = infer_request._get_output_buffer(output_b).to_numpy()

            # create blob buffers
            requests.append(infer_request)
        exec_net._requests = tuple(requests)

        return exec_net

    cpdef void set_initial_affinity(self, IENetwork net) except *:
        if self.device.find("HETERO") == -1:
            raise RuntimeError("set_initial_affinity method applicable only for HETERO device")
        self.impl.setInitialAffinity(net.impl)

    cpdef set get_supported_layers(self, IENetwork net):
        return set([l.decode() for l in self.impl.queryNetwork(net.impl)])

    @property
    def device(self):
        device_name = bytes(self.impl.device_name)
        return to_py_string(device_name)

    @property
    def version(self):
        version = bytes(self.impl.version)
        return version.decode()

    cpdef void add_cpu_extension(self, str extension_path) except *:
        if self.device.find("CPU") == -1:
            raise RuntimeError("add_cpu_extension method applicable only for CPU or HETERO devices")
        cdef string extension_str = extension_path.encode()
        self.impl.addCpuExtension(extension_str)

    cpdef void set_config(self, config):
        cdef map[string, string] c_config
        for k, v in config.items():
            c_config[to_std_string(k)] = to_std_string(v)
        self.impl.setConfig(c_config)

cdef class IENetReader:
    def read(self, model: str, weights: str) -> IENetwork:
        cdef IENetwork net = IENetwork()
        net.impl = self.impl.read(model.encode(), weights.encode())
        return net

cdef class BlobBuffer:
    """Copy-less accessor for Inference Engine Blob"""

    cdef reset(self, Blob.Ptr & ptr):
        self.ptr = ptr
        cdef TensorDesc desc = deref(ptr).getTensorDesc()
        cdef SizeVector shape = desc.getDims()
        cdef Py_ssize_t itemsize = deref(ptr).element_size()
        self.strides.resize(shape.size())
        self.shape.resize(shape.size())

        total_stride = itemsize
        # dims are in row major (C - style),
        # thence strides are computed starting from latest dimension
        for i in reversed(range(shape.size())):
            self.strides[i] = total_stride
            self.shape[i] = shape[i]
            total_stride *= shape[i]

        self.total_stride = total_stride
        self.format = self._get_blob_format(desc)
        self.item_size = itemsize

    def __getbuffer__(self, Py_buffer *buffer, int flags):
        buffer.buf = C.get_buffer[char](deref(self.ptr))
        buffer.format = self.format
        buffer.internal = NULL
        buffer.itemsize = self.item_size
        buffer.len = self.total_stride
        buffer.ndim = self.shape.size()
        buffer.obj = self
        buffer.readonly = 0
        buffer.shape = self.shape.data()
        buffer.strides = self.strides.data()
        buffer.suboffsets = NULL

    cdef char*_get_blob_format(self, const TensorDesc & desc):
        cdef Precision precision = desc.getPrecision()
        name = bytes(precision.name()).decode()
        # todo: half floats
        precision_to_format = {
            'FP32': 'f',  # float
            'FP16': 'h',  # signed short
            'Q78': 'h',  # signed short
            'I16': 'h',  # signed short
            'U8': 'B',  # unsigned char
            'I8': 'b',  # signed char
            'U16': 'H',  # unsigned short
            'I32': 'i'  # signed int
        }

        if name not in precision_to_format:
            raise ValueError("Unknown Blob precision: {}".format(name))

        return precision_to_format[name].encode()

    def to_numpy(self):
        return np.asarray(self)
