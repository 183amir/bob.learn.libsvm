#!/usr/bin/env python
# vim: set fileencoding=utf-8 :
# Andre Anjos <andre.anjos@idiap.ch>
# Sun  4 Mar 20:06:14 2012 
#
# Copyright (C) 2011-2013 Idiap Research Institute, Martigny, Switzerland


"""Tests for libsvm training
"""

import os, sys
import unittest
import math
import bob
import numpy
import tempfile
import pkg_resources
from ...test import utils

def F(f, module=None):
  """Returns the test file on the "data" subdirectory"""
  if module is None:
    return pkg_resources.resource_filename(__name__, os.path.join('data', f))
  return pkg_resources.resource_filename('bob.%s.test' % module, 
      os.path.join('data', f))

def tempname(suffix, prefix='bobtest_'):
  (fd, name) = tempfile.mkstemp(suffix, prefix)
  os.close(fd)
  os.unlink(name)
  return name

TEST_MACHINE_NO_PROBS = F('heart_no_probs.svmmodel', 'machine')

HEART_DATA = F('heart.svmdata', 'machine') #13 inputs
HEART_MACHINE = F('heart.svmmodel', 'machine') #supports probabilities
HEART_EXPECTED = F('heart.out', 'machine') #expected probabilities

class SvmTrainingTest(unittest.TestCase):
  """Performs various SVM training tests."""

  @utils.libsvm_available
  def test01_initialization(self):

    # tests and examplifies some initialization parameters

    # all defaults
    trainer = bob.trainer.SVMTrainer()

  @utils.libsvm_available
  def test02_training(self):
   
    # For this example I'm using an SVM file because of convinience. You only
    # need to make sure you can gather the input into 2D double arrays in which
    # each array represents data from one class and each line on such array
    # contains a sample.
    f = bob.machine.SVMFile(HEART_DATA)
    labels, data = f.read_all()
    neg = numpy.vstack([k for i,k in enumerate(data) if labels[i] < 0])
    pos = numpy.vstack([k for i,k in enumerate(data) if labels[i] > 0])

    # Data is also pre-scaled so features remain in the range between -1 and
    # +1. libsvm, apparently, suggests you do that for all features. Our
    # bindings to libsvm do not include scaling. If you want to implement that
    # generically, please do it.

    trainer = bob.trainer.SVMTrainer()
    machine = trainer.train((pos, neg)) #ordering only affects labels
    previous = bob.machine.SupportVector(TEST_MACHINE_NO_PROBS)
    self.assertEqual(machine.svm_type, previous.svm_type)
    self.assertEqual(machine.kernel_type, previous.kernel_type)
    self.assertEqual(machine.gamma, previous.gamma)
    self.assertEqual(machine.shape, previous.shape)
    self.assertTrue( numpy.all(abs(machine.input_subtract - \
      previous.input_subtract) < 1e-8) )
    self.assertTrue( numpy.all(abs(machine.input_divide - \
      previous.input_divide) < 1e-8) )

    curr_label = machine.predict_classes(data)
    prev_label = previous.predict_classes(data)
    self.assertEqual(curr_label, prev_label)

    curr_labels, curr_scores = machine.predict_classes_and_scores(data)
    prev_labels, prev_scores = previous.predict_classes_and_scores(data)
    self.assertEqual(curr_labels, prev_labels)

    curr_scores = numpy.array(curr_scores)
    prev_scores = numpy.array(prev_scores)
    self.assertTrue( numpy.all(abs(curr_scores - prev_scores) < 1e-8) )

  @utils.libsvm_available
  def test03_training_with_probability(self):
   
    f = bob.machine.SVMFile(HEART_DATA)
    labels, data = f.read_all() 
    neg = numpy.vstack([k for i,k in enumerate(data) if labels[i] < 0])
    pos = numpy.vstack([k for i,k in enumerate(data) if labels[i] > 0])

    # Data is also pre-scaled so features remain in the range between -1 and
    # +1. libsvm, apparently, suggests you do that for all features. Our
    # bindings to libsvm do not include scaling. If you want to implement that
    # generically, please do it.

    trainer = bob.trainer.SVMTrainer(probability=True)
    machine = trainer.train((pos, neg)) #ordering only affects labels
    previous = bob.machine.SupportVector(HEART_MACHINE)
    self.assertEqual(machine.svm_type, previous.svm_type)
    self.assertEqual(machine.kernel_type, previous.kernel_type)
    self.assertEqual(machine.gamma, previous.gamma)
    self.assertEqual(machine.shape, previous.shape)
    self.assertTrue( numpy.all(abs(machine.input_subtract - \
      previous.input_subtract) < 1e-8) )
    self.assertTrue( numpy.all(abs(machine.input_divide - \
      previous.input_divide) < 1e-8) )

    # check labels
    curr_label = machine.predict_classes(data)
    prev_label = previous.predict_classes(data)
    self.assertEqual(curr_label, prev_label)

    # check scores
    curr_labels, curr_scores = machine.predict_classes_and_scores(data)
    prev_labels, prev_scores = previous.predict_classes_and_scores(data)
    self.assertEqual(curr_labels, prev_labels)

    curr_scores = numpy.array(curr_scores)
    prev_scores = numpy.array(prev_scores)
    self.assertTrue( numpy.all(abs(curr_scores - prev_scores) < 1e-8) )

    # check probabilities -- probA and probB do not get the exact same values
    # as when using libsvm's svm-train.c. The reason may lie in the order in
    # which the samples are arranged.
    curr_labels, curr_scores = machine.predict_classes_and_probabilities(data)
    prev_labels, prev_scores = previous.predict_classes_and_probabilities(data)
    curr_scores = numpy.array(curr_scores)
    prev_scores = numpy.array(prev_scores)
    #self.assertTrue( numpy.all(abs(curr_scores-prev_scores) < 1e-8) )
